#version 410 core

layout (location = 0) in vec3 positionIn;
layout (location = 1) in vec3 colorIn;
layout (location = 2) in vec3 normalIn;
layout (location = 3) in vec2 uvsIn;

uniform mat4 projectionUniform;
uniform mat4 viewUniform;
uniform mat4 modelUniform;

out vec3 colorPass;
out vec3 normalPass;
out vec2 uvsPass;

void main()
{
    gl_Position = projectionUniform * viewUniform * modelUniform * vec4(positionIn, 1.0);

    colorPass = colorIn;
    normalPass = normalIn;
    uvsPass = uvsIn;
}