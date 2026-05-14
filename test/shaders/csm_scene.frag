#version 460

layout(set = 0, binding = 1) uniform sampler2DArray shadowMap;

layout(set = 1, binding = 0, std430) readonly buffer SceneData {
    mat4 view;
    vec4 lightDir;
    int numCascades;
    int usePCF;
    int colorCascades;
    int showExtents;
    int colorShadow;
    int cameraFrozen;
} scene;

layout(set = 1, binding = 1, std430) readonly buffer Cascades {
    mat4 cascadeViewProjMat[];
};

layout(set = 1, binding = 2, std430) readonly buffer CascadeSplits {
    float cascadeSplits[];
};

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec3 baseColor;

layout(location = 0) out vec4 fragColor;

const vec3 cascadeColors[6] = vec3[6](
    vec3(1.0, 0.25, 0.25),
    vec3(0.25, 1.0, 0.25),
    vec3(0.25, 0.25, 1.0),
    vec3(1.0, 1.0, 0.25),
    vec3(0.25, 1.0, 1.0),
    vec3(1.0, 0.25, 1.0)
);

const float Ambient = 0.24;

float shadowCalculation(vec4 lightSpacePos, uint cascadeIndex) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    if (projCoords.z > 1.0) {
        return 0.0;
    }

    float closestDepth = texture(shadowMap, vec3(projCoords.xy, cascadeIndex)).r;
    float currentDepth = projCoords.z;
    return currentDepth > closestDepth ? 1.0 : 0.0;
}

float pcfFilteredShadow(vec4 lightSpacePos, uint cascadeIndex) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    if (projCoords.z > 1.0) {
        return 0.0;
    }

    float shadow = 0.0;
    float currentDepth = projCoords.z;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0).xy;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec3 uvw = vec3(projCoords.xy + vec2(x, y) * texelSize, cascadeIndex);
            float pcfDepth = texture(shadowMap, uvw).r;
            shadow += currentDepth > pcfDepth ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

float computeShadow(out uint cascadeIndex) {
    cascadeIndex = 0;
    for (int i = 0; i < scene.numCascades - 1; ++i) {
        if (viewPos.z < cascadeSplits[i]) {
            cascadeIndex = uint(i + 1);
        }
    }

    vec4 lightSpacePos = cascadeViewProjMat[cascadeIndex] * vec4(worldPos, 1.0);
    float shadow = scene.usePCF == 1
        ? pcfFilteredShadow(lightSpacePos, cascadeIndex)
        : shadowCalculation(lightSpacePos, cascadeIndex);

    return shadow;
}

void main() {
    vec3 L = normalize(scene.lightDir.xyz);
    vec3 N = normalize(worldNormal);
    float diffuse = max(0.0, dot(N, L));
    vec3 color = (Ambient + diffuse) * baseColor;

    uint cascadeIndex;
    float shadow = computeShadow(cascadeIndex);
    color = scene.colorShadow == 0
        ? mix(color, baseColor * Ambient, shadow)
        : mix(color, baseColor * cascadeColors[cascadeIndex] * Ambient, shadow);

    if (scene.colorCascades == 1) {
        color = mix(color, cascadeColors[cascadeIndex], 0.5);
    }

    if (scene.showExtents == 1 && (abs(fract(worldPos.x) - 0.5) > 0.48 || abs(fract(worldPos.z) - 0.5) > 0.48)) {
        color = mix(color, cascadeColors[cascadeIndex], 0.35);
    }

    fragColor = vec4(color, 1.0);
}
