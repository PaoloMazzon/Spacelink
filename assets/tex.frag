#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushBuffer {
    mat4 model;
    vec4 colourMod;
} pushBuffer;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 2) uniform PostFX {
    float trip;
    float dir;
} postFX;

void main() {
    outColor = texture(texSampler, fragTexCoord);
    for (int i = 1; i <= postFX.trip; i++) {
        vec4 colour2 = texture(texSampler, fragTexCoord + vec2(cos(postFX.dir) * i, sin(postFX.dir) * i));
        vec4 color = outColor;
        outColor = vec4(
            color.r + (colour2.r * (colour2.a * (pow(0.75, i)))),
            color.g + (colour2.g * (colour2.a * (pow(0.75, i)))),
            color.b + (colour2.b * (colour2.a * (pow(0.75, i)))),
            color.a);
    }
}