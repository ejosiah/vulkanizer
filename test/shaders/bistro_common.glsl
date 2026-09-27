layout(push_constant) uniform Scene {
    mat4 matrix;
    vec4 diffuse;
    vec4 lightDirection;
    uvec4 info; // clipmap, caster index, debug mode, reserved
} scene;
