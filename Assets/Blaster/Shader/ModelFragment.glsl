#version 410 core

in VS_OUT
{
    vec2 uv;
    vec4 color;
} fs_in;

uniform sampler2D diffuse;
uniform bool hasTexture = false;

layout (location = 0) out vec4 fragColor;

void main()
{
    vec4 baseCol = hasTexture ? texture(diffuse, fs_in.uv) : fs_in.color;

    fragColor = baseCol;
}
