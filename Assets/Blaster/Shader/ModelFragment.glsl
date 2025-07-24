#version 410 core

in VS_OUT
{
    vec2 uv;
    vec4 color;
} fs_in;

uniform sampler2D diffuse;

layout (location = 0) out vec4 fragColor;

void main()
{
    vec4 baseCol = texture(diffuse, fs_in.uv);

    if(baseCol.a < 0.1)
        discard;

    fragColor = baseCol;
}