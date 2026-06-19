#version 330 core

in vec3 TexDir;                 // smerový vektor pre cubemap lookup
out vec4 FragColor;

uniform samplerCube skyboxTex;

void main()
{
    FragColor = texture(skyboxTex, normalize(TexDir)); // vzorkovanie skyboxu
}
