#version 410 core

in vec3 vColor;
in vec2 vUV;

uniform sampler2D uFontAtlas;
uniform float uThreshold = 0.5;
uniform float uSoftness = 0.1;

out vec4 FragColor;

void main()
{
    float dist = texture(uFontAtlas, vUV).r; 
    float alpha = smoothstep(uThreshold - uSoftness, uThreshold + uSoftness, dist);

    FragColor = vec4(vColor, 1.0);
    FragColor.a *= alpha;

    if (FragColor.a <= 0.0001)
        discard;
}
