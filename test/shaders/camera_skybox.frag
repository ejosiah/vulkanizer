#version 460

layout(set = 0, binding = 0) uniform samplerCube skybox;
layout(location = 0) in vec3 direction;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(skybox, normalize(direction));
}
