#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

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

layout(push_constant) uniform Constants {
    mat4 world;
    mat4 viewProjection;
} constants;

layout(location = 0) out vec3 worldPos;
layout(location = 1) out vec3 viewPos;
layout(location = 2) out vec3 worldNormal;
layout(location = 3) out vec3 baseColor;

void main() {
    vec4 worldPosition = constants.world * vec4(position, 1.0);
    worldPos = worldPosition.xyz;
    viewPos = (scene.view * worldPosition).xyz;
    worldNormal = mat3(transpose(inverse(constants.world))) * normal;
    baseColor = mix(vec3(0.55, 0.72, 0.42), vec3(0.76, 0.60, 0.44), clamp(position.y + 0.5, 0.0, 1.0));
    gl_Position = constants.viewProjection * worldPosition;
}
