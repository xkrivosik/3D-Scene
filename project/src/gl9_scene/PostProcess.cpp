#include "PostProcess.h"

#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <sstream>

// =====================================
// Globálne rozmery postprocessu
// =====================================
static int g_ppWidth  = 0;
static int g_ppHeight = 0;

// =====================================
// Pomocná funkcia: načítanie shaderu zo súboru
// =====================================
static GLuint LoadShader(const std::string& path, GLenum type)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Shader file not found: " << path << std::endl;
        return 0;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();
    const char* src = code.c_str();

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "Shader compile error in " << path << ":\n" << log << std::endl;
    }

    return shader;
}

// =====================================
// Konštruktor PostProcess
// =====================================
PostProcess::PostProcess(int width, int height,
                         const std::string& vsPath,
                         const std::string& fsPath)
        : fboHDR(0),
          colorHDR(0),
          rboDepth(0),
          quadVAO(0),
          quadVBO(0),
          shaderProgram(0)
{
    g_ppWidth  = width;
    g_ppHeight = height;

    // HDR framebuffer
    glGenFramebuffers(1, &fboHDR);
    glBindFramebuffer(GL_FRAMEBUFFER, fboHDR);

    // HDR color buffer (float)
    glGenTextures(1, &colorHDR);
    glBindTexture(GL_TEXTURE_2D, colorHDR);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA16F,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           colorHDR,
                           0);

    // Depth + stencil renderbuffer
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER,
                          GL_DEPTH24_STENCIL8,
                          width,
                          height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER,
                              rboDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "PostProcess: HDR framebuffer not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Fullscreen quad
    setupQuad();

    // Bloom + HDR shader
    loadShaders(vsPath, fsPath);
}

// =====================================
// Destruktor PostProcess
// =====================================
PostProcess::~PostProcess()
{
    if (fboHDR)        glDeleteFramebuffers(1, &fboHDR);
    if (rboDepth)      glDeleteRenderbuffers(1, &rboDepth);
    if (colorHDR)      glDeleteTextures(1, &colorHDR);
    if (quadVBO)       glDeleteBuffers(1, &quadVBO);
    if (quadVAO)       glDeleteVertexArrays(1, &quadVAO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

// =====================================
// Bind / Unbind HDR framebuffer
// =====================================
void PostProcess::BindFramebuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fboHDR);
}

void PostProcess::UnbindFramebuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// =====================================
// Render: HDR + bloom + gamma
// =====================================
void PostProcess::Render(bool useBlur, GLuint blurTex)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(shaderProgram);

    // viewport na veľkosť okna
    glViewport(0, 0, g_ppWidth, g_ppHeight);

    // HDR scéna
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorHDR);
    glUniform1i(glGetUniformLocation(shaderProgram, "sceneTex"), 0);

    // Rozmazaný bloom
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, blurTex);
    glUniform1i(glGetUniformLocation(shaderProgram, "blurTex"), 1);

    // Prepínač bluru
    glUniform1i(glGetUniformLocation(shaderProgram, "useBlur"),
                useBlur ? 1 : 0);

    // Fullscreen quad draw
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

// =====================================
// Nastavenie fullscreen quadu
// =====================================
void PostProcess::setupQuad()
{
    float quadVertices[] = {
            // pos      // tex
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
            1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
            1.0f, -1.0f,  1.0f, 0.0f,
            1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(quadVertices),
                 quadVertices,
                 GL_STATIC_DRAW);

    // aPos (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          4 * sizeof(float),
                          (void*)0);

    // aTexCoords (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          4 * sizeof(float),
                          (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

// =====================================
// Načítanie postprocess shader programu
// =====================================
void PostProcess::loadShaders(const std::string& vsPath,
                              const std::string& fsPath)
{
    GLuint vs = LoadShader(vsPath, GL_VERTEX_SHADER);
    GLuint fs = LoadShader(fsPath, GL_FRAGMENT_SHADER);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    GLint ok = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(shaderProgram, 1024, nullptr, log);
        std::cerr << "PostProcess shader link error:\n" << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}
