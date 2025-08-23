#version 410 core

in vec3 vColor;
in vec2 vUV;

uniform sampler2D uTexture; 

out vec4 FragColor;

void main()
{
    FragColor = texture(uTexture, vUV) * vec4(vColor, 1.0);

    if (FragColor.a <= 0.0001)
        discard;
}