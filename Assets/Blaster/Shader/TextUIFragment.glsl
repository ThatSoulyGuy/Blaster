#version 410 core

in vec3 vColor;
in vec2 vUV;

uniform sampler2D uFontAtlas;

out vec4 FragColor;

void main()
{
    float alpha = texture(uFontAtlas, vUV).r;
    // const float softness = 0.02;
    // alpha = smoothstep(0.0, softness, alpha);

    FragColor = vec4(vColor * alpha, alpha);
}