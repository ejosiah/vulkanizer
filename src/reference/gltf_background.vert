#version 460
layout(location = 0) out vec2 clip_position;
void main() {
    clip_position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(clip_position, 1.0, 1.0);
}
