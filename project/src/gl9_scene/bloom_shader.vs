#version 330 core

layout(location = 0) in vec2 aPos;        // fullscreen quad pozícia
layout(location = 1) in vec2 aTexCoords;  // UV súradnice

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;               // preposielame UV do fragment shaderu
    gl_Position = vec4(aPos, 0.0, 1.0);   // fullscreen pozícia
}
