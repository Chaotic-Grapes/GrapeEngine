#version 460 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;   // The LDR texture

void main()
{
    FragColor = texture(uTex, vUV);
}
