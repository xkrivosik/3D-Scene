#pragma once

static const char* PARTICLE_VERT_SHADER = R"glsl(
#version 330 core

layout(location = 0) in vec3 inPos;     // pozícia partikle
layout(location = 1) in vec3 inColor;   // farba partikle

uniform mat4 View;
uniform mat4 Projection;

out vec3 vColor;

void main() {
    vec4 clipPos = Projection * View * vec4(inPos, 1.0);
    gl_Position = clipPos;

    gl_PointSize = 6.0 * (1.0 / clipPos.w); // perspektívne zmenšovanie bodov
    vColor = inColor;
}
)glsl";


static const char* PARTICLE_FRAG_SHADER = R"glsl(
#version 330 core

in vec3 vColor;
out vec4 FragColor;

void main() {
    vec2 c = 2.0 * gl_PointCoord - 1.0;  // lokálne súradnice bodu (-1..1)
    if (dot(c, c) > 1.0) discard;        // kruhová maska
    FragColor = vec4(vColor, 1.0);
}
)glsl";
