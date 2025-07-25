#version 410 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aUV;

uniform mat4 modelUniform;
uniform mat4 projectionUniform;

out vec3 vColor;
out vec2 vUV;

void main()
{
    vColor = aColor;
    vUV = aUV;
    gl_Position = projectionUniform * modelUniform * vec4(aPos, 1.0);
}
