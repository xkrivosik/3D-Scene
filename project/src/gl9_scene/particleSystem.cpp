#include "ParticleSystem.h"
#include "particleShaders.h"

#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

// =====================================
// KONŠTRUKTOR / DEŠTRUKTOR
// =====================================
ParticleSystem::ParticleSystem() {
    std::srand((unsigned)std::time(nullptr));

    // shader programu pre particly
    program = CompileShaderProgram(PARTICLE_VERT_SHADER, PARTICLE_FRAG_SHADER);

    // ---------------- dynamické particly ----------------
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboPos);
    glGenBuffers(1, &vboColor);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float) * 4096, nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, vboColor);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float) * 4096, nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);

    // ---------------- kolízny elipsoid ----------------
    glGenVertexArrays(1, &colliderVAO);
    glGenBuffers(1, &colliderVBOPos);
    glGenBuffers(1, &colliderVBOColor);

    glBindVertexArray(colliderVAO);

    glBindBuffer(GL_ARRAY_BUFFER, colliderVBOPos);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float) * 4096, nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, colliderVBOColor);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float) * 4096, nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);

    // ---------------- OpenGL globálne nastavenia ----------------
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

ParticleSystem::~ParticleSystem() {
    if (vboPos)       glDeleteBuffers(1, &vboPos);
    if (vboColor)     glDeleteBuffers(1, &vboColor);
    if (vao)          glDeleteVertexArrays(1, &vao);

    if (colliderVBOPos)   glDeleteBuffers(1, &colliderVBOPos);
    if (colliderVBOColor) glDeleteBuffers(1, &colliderVBOColor);
    if (colliderVAO)      glDeleteVertexArrays(1, &colliderVAO);

    if (program)      glDeleteProgram(program);
}

// =====================================
// NASTAVENIA EMITERU A KOLAJDRA
// =====================================
void ParticleSystem::setEmitterPosition(const glm::vec3 &p) {
    emitterPos = p;
}

void ParticleSystem::setFishCollider(const glm::vec3 &center,
                                     const glm::vec3 &radii,
                                     const glm::vec3 &eulerDeg)
{
    fishCenter = center;
    fishRadii  = radii;

    // rotácia kolajdra z eulerov
    glm::mat4 R(1.0f);
    R = glm::rotate(R, glm::radians(eulerDeg.y), glm::vec3(0,1,0));
    R = glm::rotate(R, glm::radians(eulerDeg.x), glm::vec3(1,0,0));
    R = glm::rotate(R, glm::radians(eulerDeg.z), glm::vec3(0,0,1));

    fishRotation    = glm::mat3(R);
    hasFishCollider = true;
}

void ParticleSystem::clearFishCollider() {
    hasFishCollider = false;
}

// =====================================
// EMITOVANIE BURSTU ČASTÍC
// =====================================
void ParticleSystem::emitBurst() {
    for (int i = 0; i < spawnPerBurst; ++i) {
        Particle p;
        p.pos = emitterPos;

        // náhodná rýchlosť
        p.vel = glm::vec3(
                (std::rand() % 100 - 50) / 50.0f * 1.2f,
                (std::rand() % 100)      / 50.0f * 0.8f,
                (std::rand() % 100 - 50) / 50.0f * 1.2f
        );

        // farebná škála dymu
        float r = (std::rand() % 100) / 100.0f;
        float shade;
        if      (r < 0.05f) shade = 0.0f;
        else if (r < 0.4f)  shade = 0.3f;
        else if (r < 0.8f)  shade = 0.6f;
        else                shade = 0.8f;
        p.color = glm::vec3(shade);

        p.age      = 0.0f;
        p.lifetime = 2.0f + (std::rand() % 100) / 100.0f * 0.2f;

        p.mass = 0.5f + static_cast<float>(std::rand()) / RAND_MAX * 1.5f;

        particles.push_back(p);
    }
}

// =====================================
// UPDATE FYZIKY ČASTÍC
// =====================================
void ParticleSystem::update(float dt) {
    spawnTimer += dt;
    if (spawnTimer >= spawnInterval) {
        spawnTimer = 0.0f;
        emitBurst();
    }

    glm::vec3 gravity(0.0f, -0.8f, 0.0f);
    glm::vec3 water  (0.0f,  0.0f, -2.0f);

    float avgRadius = (fishRadii.x + fishRadii.y + fishRadii.z) / 3.0f;

    for (auto &p : particles) {
        p.age += dt;

        // akcelerácia
        glm::vec3 accel = (gravity + water) / p.mass;

        // integrácia
        p.vel += accel * dt;
        p.pos += p.vel * dt;
        p.vel *= 0.9f;

        // ---------------- kolízia ----------------
        if (hasFishCollider) {
            glm::vec3 relWorld = p.pos - fishCenter;
            glm::vec3 relLocal = glm::transpose(fishRotation) * relWorld;

            float dist = glm::length(relLocal);
            if (dist > 0.0001f && dist < avgRadius) {

                glm::vec3 nLocal = relLocal / dist;
                glm::vec3 nWorld = glm::normalize(fishRotation * nLocal);

                float vn = glm::dot(p.vel, nWorld);
                if (vn < 0.0f) {
                    p.vel = p.vel - 2.0f * vn * nWorld;
                }

                glm::vec3 surfLocal = nLocal * avgRadius;
                glm::vec3 surfWorld = fishCenter + fishRotation * surfLocal;
                p.pos = surfWorld;
            }
        }

        // dopad na dno
        if (p.pos.y < -0.03f) {
            p.pos.y = -0.03f;
            p.age = p.lifetime;
        }
    }

    // odstránenie starých častíc
    particles.erase(
            std::remove_if(particles.begin(), particles.end(),
                           [](const Particle &p){ return p.age >= p.lifetime; }),
            particles.end()
    );
}

// =====================================
// RENDER DYNAMICKÝCH ČASTÍC
// =====================================
void ParticleSystem::render(const glm::mat4 &view, const glm::mat4 &proj) {
    if (particles.empty()) return;

    std::vector<float> posData;
    std::vector<float> colData;
    posData.reserve(particles.size() * 3);
    colData.reserve(particles.size() * 3);

    for (auto &p : particles) {
        posData.push_back(p.pos.x);
        posData.push_back(p.pos.y);
        posData.push_back(p.pos.z);

        colData.push_back(p.color.r);
        colData.push_back(p.color.g);
        colData.push_back(p.color.b);
    }

    glBindVertexArray(vao);
    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glBufferData(GL_ARRAY_BUFFER, posData.size() * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, posData.size() * sizeof(float), posData.data());

    glBindBuffer(GL_ARRAY_BUFFER, vboColor);
    glBufferData(GL_ARRAY_BUFFER, colData.size() * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, colData.size() * sizeof(float), colData.data());

    glUniformMatrix4fv(glGetUniformLocation(program, "View"),       1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "Projection"), 1, GL_FALSE, &proj[0][0]);

    glDrawArrays(GL_POINTS, 0, (GLsizei)particles.size());

    glBindVertexArray(0);
    glUseProgram(0);
}

// =====================================
// RENDER KOLAJDRA (ELIPSOID)
// =====================================
void ParticleSystem::renderCollider(const glm::mat4 &view, const glm::mat4 &proj) {
    if (!hasFishCollider) return;

    const int latSteps = 24;
    const int lonSteps = 32;

    std::vector<float> posData;
    std::vector<float> colData;
    posData.reserve(latSteps * lonSteps * 3);
    colData.reserve(latSteps * lonSteps * 3);

    // generovanie bodov
    for (int i = 0; i <= latSteps; ++i) {
        float v = (float)i / (float)latSteps;
        float phi = v * glm::pi<float>() - glm::half_pi<float>();

        float cosPhi = cosf(phi);
        float sinPhi = sinf(phi);

        for (int j = 0; j < lonSteps; ++j) {
            float u = (float)j / (float)lonSteps;
            float theta = u * glm::two_pi<float>();

            glm::vec3 localUnit(
                    cosPhi * cosf(theta),
                    sinPhi,
                    cosPhi * sinf(theta)
            );

            glm::vec3 scaledLocal(
                    localUnit.x * fishRadii.x,
                    localUnit.y * fishRadii.y,
                    localUnit.z * fishRadii.z
            );

            glm::vec3 worldPos = fishCenter + fishRotation * scaledLocal;

            posData.push_back(worldPos.x);
            posData.push_back(worldPos.y);
            posData.push_back(worldPos.z);

            colData.push_back(0.0f);
            colData.push_back(1.0f);
            colData.push_back(1.0f);
        }
    }

    glBindVertexArray(colliderVAO);
    glUseProgram(program);

    glBindBuffer(GL_ARRAY_BUFFER, colliderVBOPos);
    glBufferData(GL_ARRAY_BUFFER, posData.size() * sizeof(float), posData.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, colliderVBOColor);
    glBufferData(GL_ARRAY_BUFFER, colData.size() * sizeof(float), colData.data(), GL_STREAM_DRAW);

    glUniformMatrix4fv(glGetUniformLocation(program, "View"),       1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "Projection"), 1, GL_FALSE, &proj[0][0]);

    glDisable(GL_DEPTH_TEST);
    glPointSize(5.0f);

    glDrawArrays(GL_POINTS, 0, (GLsizei)(posData.size() / 3));

    glPointSize(1.0f);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    glUseProgram(0);
}

// =====================================
// SHADER KOMPILÁCIA
// =====================================
GLuint ParticleSystem::CompileShaderProgram(const char* vertSrc, const char* fragSrc) {
    auto compile = [](GLenum type, const char* src) {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);

        GLint success;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLint len;
            glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, ' ');
            glGetShaderInfoLog(sh, len, nullptr, &log[0]);
            std::cerr << "Shader compile error: " << log << "\n";
        }
        return sh;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragSrc);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        GLint len;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, ' ');
        glGetProgramInfoLog(prog, len, nullptr, &log[0]);
        std::cerr << "Program link error: " << log << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}
