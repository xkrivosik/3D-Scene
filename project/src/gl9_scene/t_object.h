#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

// =====================================
// Trieda T_object – mesh + materiál + tiene
// =====================================
class T_object {
public:
    static const int MAX_LIGHTS = 4;

    // Zapnutie / vypnutie Blinn-Phong
    static void setUseBlinn(bool enable);

    // Texturovaný objekt
    T_object(const char* objPath, const char* texturePath);

    // Priehľadné sklo
    T_object(const char* objPath, bool makeItGlass);

    ~T_object();

    // =====================================
    // Render s parametrami (model zostaví interná funkcia)
    // =====================================
    void render(const glm::mat4& view,
                const glm::mat4& projection,
                const glm::vec3& scale    = glm::vec3(1.0f),
                const glm::vec3& position = glm::vec3(0.0f),
                const glm::vec3& rotation = glm::vec3(0.0f));

    // Render do shadow mapy
    void renderDepth(GLuint depthShader,
                     const glm::mat4& lightSpaceMatrix,
                     const glm::vec3& scale    = glm::vec3(1.0f),
                     const glm::vec3& position = glm::vec3(0.0f),
                     const glm::vec3& rotation = glm::vec3(0.0f));

    // Render s dodanou model maticou
    void renderWithModel(const glm::mat4& view,
                         const glm::mat4& projection,
                         const glm::mat4& model);

    // Depth render s dodanou model maticou
    void renderDepthWithModel(GLuint depthShader,
                              const glm::mat4& lightSpaceMatrix,
                              const glm::mat4& model);

    // Opakovanie textúry
    inline void setTexScale(const glm::vec2& s) { texScale = s; }

    // Materiál – špeculár
    inline void setSpecular(float strength, float shininess_) {
        specularStrength = strength;
        shininess        = shininess_;
    }

    // Normal mapa
    void setNormalMap(const char* normalPath);

    // Nastavenie svetiel pre všetky objekty
    static void setLights(const std::vector<glm::vec3>& positions,
                          const std::vector<glm::vec3>& colors,
                          const glm::vec3& viewPos);

    // Light-space matice (Slnko + lampa)
    static void setLightSpaceMatrices(const glm::mat4& lightSpace0,
                                      const glm::mat4& lightSpace1);

    // Shadow mapy
    static void setShadowMaps(GLuint shadow0, bool use0,
                              GLuint shadow1, bool use1);

private:

    // VAO / VBO / shader / textúra
    GLuint VAO = 0, VBO = 0;
    GLuint shaderProgram = 0;
    GLuint textureID = 0;
    GLuint vertexCount = 0;

    // Materiál a shading
    static bool s_useBlinn;
    bool isGlass = false;
    glm::vec2 texScale = glm::vec2(1.0f);
    float specularStrength = 0.3f;
    float shininess = 16.0f;

    // Normal mapa
    GLuint normalTex = 0;
    bool hasNormalMap = false;

    // Globálne svetlá
    static glm::vec3 s_lightPositions[MAX_LIGHTS];
    static glm::vec3 s_lightColors[MAX_LIGHTS];
    static int       s_lightCount;
    static glm::vec3 s_viewPos;

    // Light-space matice
    static glm::mat4 s_lightSpace0;
    static glm::mat4 s_lightSpace1;

    // Shadow mapy
    static GLuint s_shadowMap0;
    static GLuint s_shadowMap1;
    static bool   s_useShadow0;
    static bool   s_useShadow1;

    // Načítanie textúr a modelov
    GLuint loadBMP(const char* path);     // 24-bit BMP loader
    void   loadOBJ(const char* path);     // OBJ geometria
    void   createTexturedShader();        // shader pre mesh + textúra + svetlá
    void   createGlassShader();           // jednoduchý transparentný shader
};
