#pragma once
#include <GL/glew.h>
#include <string>

// =====================================
// PostProcess – HDR + bloom + fullscreen quad
// =====================================
class PostProcess {
public:
    // vytvorí HDR framebuffer + postprocess shader
    PostProcess(int width, int height,
                const std::string& vsPath,
                const std::string& fsPath);
    ~PostProcess();

    // renderovanie scény do HDR FBO
    void BindFramebuffer();
    void UnbindFramebuffer();

    // useBlur = či je aktívny BLUR (interval)
    // blurTex = textúra rozmazanej scény
    void Render(bool useBlur, GLuint blurTex);

    // prístup k FBO a HDR textúre
    GLuint getFBO() const { return fboHDR; }
    GLuint getColorTexture() const { return colorHDR; }

private:
    // HDR framebuffer + textúra + depth
    GLuint fboHDR;
    GLuint colorHDR;
    GLuint rboDepth;

    // fullscreen quad
    GLuint quadVAO, quadVBO;

    // postprocess shader (tone-mapping + bloom)
    GLuint shaderProgram;

    void setupQuad();
    void loadShaders(const std::string& vsPath, const std::string& fsPath);
};
