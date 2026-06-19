#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexDir;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    mat4 rotView = mat4(mat3(view));          // bez translácie
    vec4 pos = projection * rotView * vec4(aPos * 50.0, 1.0);
    gl_Position = pos.xyww;                   // depth = 1
    TexDir = aPos;                            // smer pre cubemap
}
