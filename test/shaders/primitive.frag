#version 460

layout(location = 0) in vec3 worldNormal;
layout(location = 1) in vec4 vertexColor;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 normal = normalize(worldNormal);
    vec3 lightDir = normalize(vec3(0.45, 0.75, 0.35));
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 color = vertexColor.rgb * (0.22 + diffuse * 0.78);
    fragColor = vec4(color, vertexColor.a);
}
