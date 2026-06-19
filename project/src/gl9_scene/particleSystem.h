#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

// =====================================
// Štruktúra jednej častice
// =====================================
struct Particle {
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 color;
    float age      = 0.0f;
    float lifetime = 0.05f;
    float mass     = 1.0f;
};

// =====================================
// Hlavná trieda systému častíc
// =====================================
class ParticleSystem {
public:

    // =====================================
    // Inicializácia / deštrukcia
    // =====================================
    ParticleSystem();
    ~ParticleSystem();

    // =====================================
    // Nastavenie pozície emiteru
    // =====================================
    void setEmitterPosition(const glm::vec3 &p);

    // =====================================
    // Nastavenie elipsoidálneho kolajdra ryby
    // =====================================
    void setFishCollider(const glm::vec3 &center,
                         const glm::vec3 &radii,
                         const glm::vec3 &eulerDeg);

    // =====================================
    // Vypnutie kolajdra
    // =====================================
    void clearFishCollider();

    // =====================================
    // Fyzika častíc
    // =====================================
    void update(float dt);

    // =====================================
    // Render dynamických častíc
    // =====================================
    void render(const glm::mat4 &view, const glm::mat4 &proj);

    // =====================================
    // Render vizualizácie kolajdra
    // =====================================
    void renderCollider(const glm::mat4 &view, const glm::mat4 &proj);

private:

    // =====================================
    // Dáta častíc
    // =====================================
    std::vector<Particle> particles;
    glm::vec3 emitterPos{0.0f};

    // spawn nastavenia
    float spawnTimer    = 0.0f;
    float spawnInterval = 1.1f;
    int   spawnPerBurst = 300;

    // =====================================
    // GPU buffery dynamických častíc
    // =====================================
    GLuint vao      = 0;
    GLuint vboPos   = 0;
    GLuint vboColor = 0;

    // =====================================
    // GPU buffery vizualizácie kolajdra
    // =====================================
    GLuint colliderVAO      = 0;
    GLuint colliderVBOPos   = 0;
    GLuint colliderVBOColor = 0;

    // =====================================
    // Shader programu pre častice
    // =====================================
    GLuint program = 0;

    // =====================================
    // Dáta ryby a kolajdra
    // =====================================
    bool      hasFishCollider = false;
    glm::vec3 fishCenter{0.0f};
    glm::vec3 fishRadii{1.0f, 1.0f, 1.0f};
    glm::mat3 fishRotation{1.0f};

    // =====================================
    // Emisia burstu častíc
    // =====================================
    void emitBurst();

    // =====================================
    // Kompilácia shader programu
    // =====================================
    GLuint CompileShaderProgram(const char* vertSrc, const char* fragSrc);
};
