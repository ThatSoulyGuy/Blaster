#version 410 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aUV;
layout (location = 3) in ivec4 aBoneIDs;
layout (location = 4) in vec4 aWeights;

uniform mat4 modelUniform;
uniform mat4 viewUniform;
uniform mat4 projectionUniform;

uniform bool uUseSkinning = false;
uniform mat4 uBoneMatrices[128];

out VS_OUT
{
    vec2 uv;
    vec4 color;
} vs_out;

mat4 skinMatrix()
{
    if (!uUseSkinning)
        return mat4(1.0);

    return aWeights.x * uBoneMatrices[aBoneIDs.x] + aWeights.y * uBoneMatrices[aBoneIDs.y] + aWeights.z * uBoneMatrices[aBoneIDs.z] + aWeights.w * uBoneMatrices[aBoneIDs.w];
}

void main()
{
    vec4 skinnedPos = skinMatrix() * vec4(aPosition, 1.0);

    vs_out.uv = aUV;
    vs_out.color = aColor;

    gl_Position = projectionUniform * viewUniform * modelUniform * skinnedPos;
}