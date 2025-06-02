#version 410 core

out vec4 FragColor;

in vec3 colorPass;
in vec3 normalPass;
in vec2 uvsPass;

void main()
{
    FragColor = vec4(colorPass, 1.0);
}