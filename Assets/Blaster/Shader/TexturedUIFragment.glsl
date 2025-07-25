#version 410 core

in vec3 vColor;
in vec2 vUV;

uniform sampler2D uTexture; 

out vec4 FragColor;

void main()
{
    vec4 tex = texture(uTexture, vUV);

    FragColor = vec4(vColor, 1.0) * tex;

    if (FragColor.a <= 0.0001)
        discard;
}