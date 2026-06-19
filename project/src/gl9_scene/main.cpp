#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <glm/gtx/string_cast.hpp>
#include <vector>
#include <fstream>
#include <sstream>
#include <functional>
#include <random>

#include "Camera.h"
#include "t_object.h"
#include "ParticleSystem.h"
#include "PostProcess.h"
#include "animation.h"
#include "rigid.h"
#include "cameraKeyFrame.h"

// =====================================
// Window settings
// =====================================
const unsigned int SCR_WIDTH  = 1920;
const unsigned int SCR_HEIGHT = 1080;

// Shadow map rozlíšenie
const unsigned int SHADOW_WIDTH  = 2048;
const unsigned int SHADOW_HEIGHT = 2048;

// =====================================
// Camera
// =====================================
Camera camera(glm::vec3(-3.04f, 1.63f, 5.08f));
float lastX = SCR_WIDTH  / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// prepínanie kamery
bool useKeyframeCamera = true; // true = path, false = free (WASD + myš)
bool g_useBlinn = true;
bool g_printCoords = false;

// =====================================
// Postprocess ON/OFF
// =====================================
bool g_postEnabled = true; // defaultne zapnutý

// =====================================
// BLUR resources (FBO, shader, quad)
// =====================================
GLuint gBlurFBO      = 0;
GLuint gBlurColorTex = 0;
GLuint gBlurProgram  = 0;
GLuint gBlurVAO      = 0;
GLuint gBlurVBO      = 0;

// =====================================
// SKYBOX resources
// =====================================
GLuint gSkyboxVAO      = 0;
GLuint gSkyboxVBO      = 0;
GLuint gSkyboxProgram  = 0;
GLuint gSkyboxTexture  = 0;

// =====================================
// Pebble instances
// =====================================
const int NUM_PEBBLES = 5000;
std::vector<glm::mat4> g_pebbleLocal;  // lokálne transformácie voči room


const float AQUA_MIN_X = -31.35f;
const float AQUA_MAX_X = -17.60f;
const float AQUA_MIN_Y =  -4.60f;
const float AQUA_MAX_Y =   7.30f;
const float AQUA_MIN_Z = -13.30f;
const float AQUA_MAX_Z =  20.80f;
const float AQUA_MARGIN_XZ = 0.3f;

// =====================================
// Procedurálne ryby a rastliny
// =====================================
struct ProcFish {
    T_object* obj;      // ktorá mesh ryba (fish, fish2, fish3, ...)
    glm::vec3 pos;      // aktuálna pozícia vo WORLD priestore
    glm::vec3 vel;      // rýchlosť vo WORLD priestore
    float     scale;    // uniform scale
    glm::mat4 model;    // world transform na render
};

struct ProcPlant {
    T_object* obj;      // plant / grass / coral
    float     scale;
    glm::mat4 model;    // world transform (rastliny sú statické)
};

const int NUM_PROC_FISH   = 120;
const int NUM_PROC_PLANTS = 40;

std::vector<ProcFish>  g_procFish;
std::vector<ProcPlant> g_procPlants;

// =====================================
// Mouse callback
// =====================================
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (useKeyframeCamera)
        return;

    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos; // inverted Y

    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.processMouseMovement(xoffset, yoffset);
}

// =====================================
// Keyboard input
// =====================================
void processInput(GLFWwindow *window, float dt)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (useKeyframeCamera)
        return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.moveForward(dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.moveBackward(dt);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.moveLeft(dt);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.moveRight(dt);

    // Debug camera print
    static bool spaceLast = false;
    bool spaceNow = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spaceNow && !spaceLast) {
        std::cout << "Camera pos: "   << glm::to_string(camera.getPosition()) << "\n";
        std::cout << "Camera front: " << glm::to_string(camera.getFront())    << "\n";
    }
    spaceLast = spaceNow;
}

// =====================================
// Framebuffer resize callback
// =====================================
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// =====================================
// Catmull-Rom helper pre trajektórie
// =====================================
glm::vec3 CatmullRom(const glm::vec3 &p0,
                     const glm::vec3 &p1,
                     const glm::vec3 &p2,
                     const glm::vec3 &p3,
                     float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f*p0 - 5.0f*p1 + 4.0f*p2 - p3) * t2 +
                   (-p0 + 3.0f*p1 - 3.0f*p2 + p3) * t3);
}

// =====================================
// Interpolácia kamery (Catmull-Rom)
// =====================================
glm::vec3 interpolateCatmullRom(const std::vector<CameraKeyframe>& keys, float t, bool isPosition) {
    float time = std::fmod(t, camAnimDuration);
    if (time < 0) time += camAnimDuration;

    // Nájdi segment
    size_t i = 0;
    for (; i < keys.size() - 1; ++i) {
        if (time <= keys[i + 1].time) break;
    }

    // Zabezpeč periodicitu (loop)
    int p0 = (i == 0) ? (keys.size() - 1) : (int(i) - 1);
    int p1 = (int)i;
    int p2 = (int)i + 1;
    int p3 = (i + 2 >= (int)keys.size()) ? (int)(i + 2 - keys.size()) : (int)i + 2;

    float t1 = keys[p1].time;
    float t2_ = keys[p2].time;

    float segmentT = (time - t1) / (t2_ - t1);

    glm::vec3 point = isPosition ? keys[p1].position : keys[p1].lookAt;
    glm::vec3 prev  = isPosition ? keys[p0].position : keys[p0].lookAt;
    glm::vec3 next  = isPosition ? keys[p2].position : keys[p2].lookAt;
    glm::vec3 next2 = isPosition ? keys[p3].position : keys[p3].lookAt;

    return CatmullRom(prev, point, next, next2, segmentT);
}

// =======================
// Shadow map shadery (jednoduchý depth pass)
// =======================
const char* vsDepthSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
)";

const char* fsDepthSrc = R"(
#version 330 core
void main()
{
    // nič – zapisuje sa iba hĺbka
}
)";

// =======================
// Sun / Moon parametric animation
// =======================

float sunParam       = 0.5f;
bool  sunAnimating   = false;
int   sunCycleState  = 0;

const float SUN_BASE_SPEED = 0.2f;

// =====================================
// Výpočet pozície Slnka/Mesiaca z parametra
// =====================================
glm::vec3 computeSunPosFromParam(float p)
{
    float normP = std::fmod(p, 2.0f);
    if (normP < 0.0f) normP += 2.0f;

    float theta;

    const float R = 45.0f;
    const glm::vec3 pivot(75.0f, 0.0f, 0.0f);

    if (normP < 0.5f) {
        float t = normP / 0.5f; // 0..1
        theta = -glm::half_pi<float>() + glm::pi<float>() * t; // -90 -> 90
    }
    else if (normP <= 1.5f) {
        float t = (normP - 0.5f); // 0..1
        theta = glm::half_pi<float>() + 2.0f * glm::pi<float>() * t; // 90 -> 450
    }
    else {
        float t = (normP - 1.5f) / 0.5f; // 0..1
        theta = glm::half_pi<float>() + 2.0f * glm::pi<float>() + glm::pi<float>() * t; // 450 -> 630
    }

    float s = std::sin(theta);
    float c = std::cos(theta);

    glm::vec3 pos;
    pos.x = pivot.x;
    pos.y = pivot.y + R * s;
    pos.z = pivot.z + R * c;

    return pos;
}

// =======================
// Scene graph – hierarchická reprezentácia
// =======================
struct SceneNode {
    T_object* object;
    glm::mat4 localTransform;
    std::vector<SceneNode*> children;
};

// =====================================
// Rekurzívny render – depth/shadow pass
// =====================================
void drawSceneNodeDepth(
        SceneNode* node,
        const glm::mat4& parentTransform,
        GLuint depthShader,
        const glm::mat4& lightSpaceMatrix,
        const std::function<bool(const T_object*)>& isTransparent
)
{
    glm::mat4 model = parentTransform * node->localTransform;

    if (node->object && !isTransparent(node->object)) {
        node->object->renderDepthWithModel(depthShader, lightSpaceMatrix, model);
    }

    for (SceneNode* child : node->children) {
        drawSceneNodeDepth(child, model, depthShader, lightSpaceMatrix, isTransparent);
    }
}

// =====================================
// Vytvorenie depth shader programu
// =====================================
static GLuint createDepthShader()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsDepthSrc, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsDepthSrc, nullptr);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

// =====================================
// Keyframe animácia pre fish5
// =====================================
void updateFish5Animation(float currentTime, const std::vector<Keyframe>& keyframes, glm::vec3& outPos, glm::vec3& outRot) {
    float animTime = fmod(currentTime, keyframes.back().time);

    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        if (animTime >= keyframes[i].time && animTime <= keyframes[i + 1].time) {
            float t = (animTime - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);

            outPos = glm::mix(keyframes[i].position, keyframes[i + 1].position, t);
            outRot = glm::mix(keyframes[i].rotation, keyframes[i + 1].rotation, t);

            return;
        }
    }

    outPos = keyframes.back().position;
    outRot = keyframes.back().rotation;
}

// Globálny čas štartu aplikácie
float startTime = 0.0f; // nastavíme po inicializácii GLFW

// =====================================
// Shader loader pre blur
// =====================================
GLuint LoadShaderFile(const std::string& path, GLenum type)
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

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "Shader compile error in " << path << ":\n" << log << std::endl;
    }
    return shader;
}

// =====================================
// Init blur resources (FBO + quad + shader)
// =====================================
void InitBlurResources(int width, int height)
{
    // 1) FBO + color texture
    glGenFramebuffers(1, &gBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gBlurFBO);

    glGenTextures(1, &gBlurColorTex);
    glBindTexture(GL_TEXTURE_2D, gBlurColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0,
                 GL_RGBA16F,
                 width, height,
                 0, GL_RGBA, GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           gBlurColorTex,
                           0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Blur FBO not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2) full-screen quad
    float quadVertices[] = {
            // pos      // tex
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
            1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
            1.0f, -1.0f,  1.0f, 0.0f,
            1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &gBlurVAO);
    glGenBuffers(1, &gBlurVBO);

    glBindVertexArray(gBlurVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gBlurVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(quadVertices),
                 quadVertices,
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float),
                          (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    // 3) BLUR shader (blur_shader.vs/fs)
    GLuint vs = LoadShaderFile("../src/gl9_scene/blur_shader.vs", GL_VERTEX_SHADER);
    GLuint fs = LoadShaderFile("../src/gl9_scene/blur_shader.fs", GL_FRAGMENT_SHADER);
    gBlurProgram = glCreateProgram();
    glAttachShader(gBlurProgram, vs);
    glAttachShader(gBlurProgram, fs);
    glLinkProgram(gBlurProgram);

    GLint ok;
    glGetProgramiv(gBlurProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(gBlurProgram, 1024, nullptr, log);
        std::cerr << "Blur shader link error:\n" << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

// -----------------------------------------------------------
// Skybox loader
// -----------------------------------------------------------
GLuint LoadSkyboxFromCrossBMP(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Skybox BMP not found: " << path << "\n";
        return 0;
    }

    unsigned char header[54];
    file.read(reinterpret_cast<char*>(header), 54);
    if (file.gcount() != 54 || header[0] != 'B' || header[1] != 'M') {
        std::cerr << "Not a valid BMP file: " << path << "\n";
        return 0;
    }

    unsigned int dataPos   = *reinterpret_cast<uint32_t*>(&header[0x0A]);
    unsigned int imageSize = *reinterpret_cast<uint32_t*>(&header[0x22]);
    unsigned int width     = *reinterpret_cast<uint32_t*>(&header[0x12]);
    unsigned int height    = *reinterpret_cast<uint32_t*>(&header[0x16]);
    unsigned short bpp     = *reinterpret_cast<uint16_t*>(&header[0x1C]);
    unsigned int compression = *reinterpret_cast<uint32_t*>(&header[0x1E]);

    if (bpp != 24 || compression != 0) {
        std::cerr << "Only 24-bit uncompressed BMP is supported (got "
                  << bpp << " bpp, compression=" << compression
                  << ") in " << path << "\n";
        return 0;
    }

    if (imageSize == 0) imageSize = width * height * 3;
    if (dataPos   == 0) dataPos   = 54;

    std::vector<unsigned char> rawData(imageSize);
    file.seekg(dataPos, std::ios::beg);
    file.read(reinterpret_cast<char*>(rawData.data()), imageSize);
    file.close();

    // BMP je bottom-up, prehodíme riadky aby sme mali (0,0) hore vľavo
    std::vector<unsigned char> imgData(width * height * 3);
    const unsigned int rowSize = (width * 3 + 3) & ~3u; // zarovnanie na 4B

    for (unsigned int y = 0; y < height; ++y) {
        unsigned int srcY = height - 1 - y; // zdola nahor
        const unsigned char* srcRow = rawData.data() + srcY * rowSize;
        unsigned char* dstRow = imgData.data() + y * width * 3;
        for (unsigned int x = 0; x < width; ++x) {
            // BGR -> RGB
            dstRow[x*3 + 0] = srcRow[x*3 + 2];
            dstRow[x*3 + 1] = srcRow[x*3 + 1];
            dstRow[x*3 + 2] = srcRow[x*3 + 0];
        }
    }

    // očakávame 4×3 dlaždice
    if (width % 4 != 0 || height % 3 != 0) {
        std::cerr << "Skybox cross must be 4x3 tiles (got "
                  << width << "x" << height << ")\n";
        return 0;
    }

    const int faceW = width / 4;
    const int faceH = height / 3;

    auto extractFace = [&](int tileX, int tileY) -> std::vector<unsigned char>
    {
        std::vector<unsigned char> face(faceW * faceH * 3);
        for (int y = 0; y < faceH; ++y) {
            int srcY = tileY * faceH + y;
            for (int x = 0; x < faceW; ++x) {
                int srcX = tileX * faceW + x;
                const unsigned char* src =
                        &imgData[(srcY * width + srcX) * 3];
                unsigned char* dst =
                        &face[(y * faceW + x) * 3];
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
            }
        }
        return face;
    };

    // vytvoríme cube mapu
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

    // mapovanie dlaždíc na jednotlivé faces:
    //  row 0:      [   ] [ +Y ] [   ] [   ]
    //  row 1:      [ -X ] [ +Z ] [ +X ] [ -Z ]
    //  row 2:      [   ] [ -Y ] [   ] [   ]

    std::vector<unsigned char> posX = extractFace(2, 1);
    std::vector<unsigned char> negX = extractFace(0, 1);
    std::vector<unsigned char> posY = extractFace(1, 0);
    std::vector<unsigned char> negY = extractFace(1, 2);
    std::vector<unsigned char> posZ = extractFace(1, 1);
    std::vector<unsigned char> negZ = extractFace(3, 1);

    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB,
                 faceW, faceH, 0, GL_RGB, GL_UNSIGNED_BYTE, posX.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB,
                 faceW, faceH, 0, GL_RGB, GL_UNSIGNED_BYTE, negX.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB,
                 faceW, faceH, 0, GL_RGB, GL_UNSIGNED_BYTE, posY.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB,
                 faceW, faceH, 0, GL_RGB, GL_UNSIGNED_BYTE, negY.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB,
                 faceW, faceH, 0, GL_RGB, GL_UNSIGNED_BYTE, posZ.data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB,
                 faceW, faceH, 0, GL_RGB, GL_UNSIGNED_BYTE, negZ.data());

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return texID;
}

// =====================================
// Program loader zo súborov (VS+FS)
// =====================================
GLuint CreateProgramFromFiles(const std::string& vsPath,
                              const std::string& fsPath)
{
    GLuint vs = LoadShaderFile(vsPath, GL_VERTEX_SHADER);
    GLuint fs = LoadShaderFile(fsPath, GL_FRAGMENT_SHADER);

    if (!vs || !fs) {
        std::cerr << "Failed to load shaders: "
                  << vsPath << " / " << fsPath << std::endl;
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::cerr << "Program link error (" << vsPath << ", " << fsPath << "):\n"
                  << log << std::endl;
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

// =====================================
// SKYBOX – inicializácia
// =====================================
void InitSkybox()
{
    // Kocka okolo kamery – iba pozície
    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,

            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,

            1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,

            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,

            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,

            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f
    };

    // VAO/VBO
    glGenVertexArrays(1, &gSkyboxVAO);
    glGenBuffers(1, &gSkyboxVBO);
    glBindVertexArray(gSkyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gSkyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Shader program zo súborov
    gSkyboxProgram = CreateProgramFromFiles(
            "../src/gl9_scene/skybox.vs",
            "../src/gl9_scene/skybox.fs"
    );
    if (!gSkyboxProgram) {
        std::cerr << "Failed to create skybox program\n";
        return;
    }

    // Cube map z cross BMP
    gSkyboxTexture = LoadSkyboxFromCrossBMP("../src/gl9_scene/textures/skybox.bmp");
    if (!gSkyboxTexture) {
        std::cerr << "WARNING: skybox texture not loaded, background will be black.\n";
    }

    // Nastavíme sampler na jednotku 0
    glUseProgram(gSkyboxProgram);
    GLint loc = glGetUniformLocation(gSkyboxProgram, "skyboxTex");
    if (loc >= 0)
        glUniform1i(loc, 0);
    glUseProgram(0);
}

// =====================================
// SKYBOX – render
// =====================================
void RenderSkybox(const glm::mat4& view, const glm::mat4& projection)
{
    if (!gSkyboxProgram || !gSkyboxVAO || !gSkyboxTexture)
        return;

    glDepthFunc(GL_LEQUAL);
    glUseProgram(gSkyboxProgram);

    glUniformMatrix4fv(glGetUniformLocation(gSkyboxProgram, "view"),
                       1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gSkyboxProgram, "projection"),
                       1, GL_FALSE, &projection[0][0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gSkyboxTexture);

    glBindVertexArray(gSkyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glUseProgram(0);
    glDepthFunc(GL_LESS);
}

// =======================
// Main
// =======================
int main() {
    // =====================================
    // GLFW / GLEW inicializácia
    // =====================================
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Aquarium", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed\n"; return -1; }
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    startTime = (float)glfwGetTime();

    // Capture mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Register mouse callback
    glfwSetCursorPosCallback(window, mouse_callback);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // SKYBOX musí byť po inicializácii OpenGL
    InitSkybox();

    // ─────────────────────────────────────────────────────────
    // Shadow map framebuffery + depth textúry (Slnko + lampa)
    // ─────────────────────────────────────────────────────────
    GLuint sunDepthFBO, sunDepthMap;
    glGenFramebuffers(1, &sunDepthFBO);
    glGenTextures(1, &sunDepthMap);
    glBindTexture(GL_TEXTURE_2D, sunDepthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, sunDepthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sunDepthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLuint lampDepthFBO, lampDepthMap;
    glGenFramebuffers(1, &lampDepthFBO);
    glGenTextures(1, &lampDepthMap);
    glBindTexture(GL_TEXTURE_2D, lampDepthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, lampDepthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, lampDepthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint depthShader = createDepthShader();

    // ─────────────────────────────────────────────────────────
    // Scénové objekty
    // ─────────────────────────────────────────────────────────
    RigidBody diverBody;
    T_object table("../src/gl9_scene/objects/table.obj", "../src/gl9_scene/textures/table_texture.bmp");
    T_object sand("../src/gl9_scene/objects/sand.obj", "../src/gl9_scene/textures/sand.bmp");
    T_object fish("../src/gl9_scene/objects/fish.obj", "../src/gl9_scene/textures/fish.bmp");
    T_object fish2("../src/gl9_scene/objects/fish2.obj", "../src/gl9_scene/textures/fish2.bmp");
    T_object fish3("../src/gl9_scene/objects/fish3.obj", "../src/gl9_scene/textures/fish3.bmp");
    T_object fish4("../src/gl9_scene/objects/fish4.obj", "../src/gl9_scene/textures/fish4.bmp");
    T_object fish5("../src/gl9_scene/objects/jelly.obj", "../src/gl9_scene/textures/coral.bmp");
    T_object chest("../src/gl9_scene/objects/chest.obj", "../src/gl9_scene/textures/chest.bmp");
    T_object pebble("../src/gl9_scene/objects/pebble.obj", "../src/gl9_scene/textures/pebble.bmp");
    T_object glass("../src/gl9_scene/objects/glass.obj", true);
    T_object glass_pane("../src/gl9_scene/objects/pane.obj", true);
    T_object glass_pane2("../src/gl9_scene/objects/pane.obj", true);
    T_object box("../src/gl9_scene/objects/box.obj", true);
    T_object plant("../src/gl9_scene/objects/plant.obj", "../src/gl9_scene/textures/plant.bmp");
    T_object grass ("../src/gl9_scene/objects/grass.obj", "../src/gl9_scene/textures/grass.bmp");
    T_object coral ("../src/gl9_scene/objects/coral.obj", "../src/gl9_scene/textures/coral.bmp");
    T_object room ("../src/gl9_scene/objects/room.obj", "../src/gl9_scene/textures/room_texture.bmp");
    T_object floorObj ("../src/gl9_scene/objects/pane_flat.obj", "../src/gl9_scene/textures/floor_texture.bmp");
    T_object roof ("../src/gl9_scene/objects/pane_flat.obj", "../src/gl9_scene/textures/roof_texture.bmp");
    T_object sand_big("../src/gl9_scene/objects/pane_flat.obj", "../src/gl9_scene/textures/sand2.bmp");
    sand_big.setNormalMap("../src/gl9_scene/textures/NormalMap.bmp");
    T_object sunObj("../src/gl9_scene/objects/sun.obj", "../src/gl9_scene/textures/sun_texture.bmp");
    T_object lamp("../src/gl9_scene/objects/lamp.obj", "../src/gl9_scene/textures/lamp.bmp");
    T_object diver("../src/gl9_scene/objects/diver.obj", "../src/gl9_scene/textures/diver.bmp");
    T_object lamp_a("../src/gl9_scene/objects/lamp_a.obj", "../src/gl9_scene/textures/lamp_a.bmp");

    // POSTPROCESS – bloom + HDR + gamma
    PostProcess postProcess(
            SCR_WIDTH, SCR_HEIGHT,
            "../src/gl9_scene/bloom_shader.vs",
            "../src/gl9_scene/bloom_shader.fs"
    );

    // BLUR resources (z HDR → rozmazaná scéna)
    InitBlurResources(SCR_WIDTH, SCR_HEIGHT);

    ParticleSystem particleSystem;

    // =====================================
    // Diver – počiatočná fyzika
    // =====================================
    diverBody.position = glm::vec3(0.5f, 2.0f, 2.0f);
    diverBody.velocity = glm::vec3(0.5f, 0.0f, 0.3f);

    room.setTexScale(glm::vec2(24.0f, 24.0f));
    floorObj.setTexScale(glm::vec2(12.0f, 12.0f));
    roof.setTexScale(glm::vec2(12.0f, 12.0f));
    sand_big.setTexScale(glm::vec2 (12.0f, 12.0f));

    // =====================================
    // Materiály (špeculárne vlastnosti)
    // =====================================
    table.setSpecular(0.1f, 8.0f);
    sand.setSpecular(0.0f, 1.0f);
    sand_big.setSpecular(0.0f, 1.0f);
    chest.setSpecular(0.3f, 16.0f);
    fish.setSpecular(0.5f, 32.0f);
    fish2.setSpecular(0.5f, 32.0f);
    fish3.setSpecular(0.5f, 32.0f);
    fish4.setSpecular(0.5f, 32.0f);
    fish5.setSpecular(0.5f, 32.0f);
    diver.setSpecular(0.05f, 32.0f);
    plant.setSpecular(0.2f, 16.0f);
    grass.setSpecular(0.2f, 16.0f);
    coral.setSpecular(0.3f, 16.0f);
    room.setSpecular(0.05f, 1.0f);
    floorObj.setSpecular(0.2f, 16.0f);
    roof.setSpecular(0.2f, 16.0f);
    sunObj.setSpecular(0.8f, 64.0f);
    lamp.setSpecular(0.8f, 32.0f);
    lamp_a.setSpecular(0.8f, 32.0f);

    float lastFrameTime = glfwGetTime();

    // =====================================
    // Trajektória pre fish3 (Catmull-Rom)
    // =====================================
    std::vector<glm::vec3> fish3Points = {
            {1.0f, 2.0f, 3.0f},
            {1.7f, 2.0f, 2.0f},
            {1.0f, 1.9f, 1.0f},
            {0.0f, 2.1f, 2.0f}
    };

    glm::vec3 fish4Pos = glm::vec3(1.0f, 1.5f, 2.0f);
    glm::vec3 fish4Vel = glm::vec3(0.4f, 0.2f, 0.3f);

    static float tFish1 = 0.0f;
    static bool  forwardFish1 = true;
    static float tFish3 = 0.0f;

    std::vector<Keyframe> fish5Keyframes = {
            {0.0f, glm::vec3(-1.0f, 1.5f, 2.0f), glm::vec3(0.0f, 90.0f, 0.0f)},
            {4.0f, glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.0f, 180.0f, 0.0f)},
            {8.0f, glm::vec3(1.0f, 1.5f, 2.0f), glm::vec3(0.0f, 270.0f, 0.0f)},
            {12.0f, glm::vec3(0.0f, 1.5f, 3.0f), glm::vec3(0.0f, 360.0f, 0.0f)},
            {16.0f, glm::vec3(-1.0f, 1.5f, 2.0f), glm::vec3(0.0f, 450.0f, 0.0f)}
    };

    float fish5AnimDuration = fish5Keyframes.back().time;
    (void)fish5AnimDuration;

    float fish5CurrentTime = 0.0f;
    glm::vec3 fish5Pos = fish5Keyframes[0].position;
    glm::vec3 fish5Rot = fish5Keyframes[0].rotation;

    // =====================================
    // Pomocný lambda na tvorbu model matíc
    // =====================================
    auto makeModel = [](const glm::vec3& scale,
                        const glm::vec3& pos,
                        const glm::vec3& rotDeg)
    {
        glm::mat4 m(1.0f);
        m = glm::translate(m, pos);
        m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1,0,0));
        m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0,1,0));
        m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0,0,1));
        m = glm::scale(m, scale);
        return m;
    };

    // --- statické world transformácie ---
    glm::mat4 roomWorld   = makeModel(glm::vec3(2.0f),
                                      glm::vec3(-13.2f, -6.3f, 4.0f),
                                      glm::vec3(0.0f, 90.0f, 0.0f));

    glm::mat4 floorWorld  = makeModel(glm::vec3(3.0f,0.5f,3.0f),
                                      glm::vec3(-21.0f, -15.15f, 21.5f),
                                      glm::vec3(0.0f));
    glm::mat4 roofWorld   = makeModel(glm::vec3(3.0f,0.5f,3.0f),
                                      glm::vec3(-22.0f, -2.0f, 24.0f),
                                      glm::vec3(0.0f));
    glm::mat4 sandBigWorld = makeModel(glm::vec3(0.85f,0.5f,2.0f),
                                       glm::vec3(-25.0f, -14.85f, 21.5f),
                                       glm::vec3(0.0f));

    glm::mat4 tableWorld  = makeModel(glm::vec3(0.05f),
                                      glm::vec3(0.0f, -5.0f, 1.0f),
                                      glm::vec3(0.0f, 90.0f, 0.0f));

    glm::mat4 boxWorld    = makeModel(glm::vec3(4.4f, 4.0f, 6.4f),
                                      glm::vec3(0.0f, 0.0f, 1.4f),
                                      glm::vec3(0.0f));
    glm::mat4 glassWorld  = makeModel(glm::vec3(0.1f),
                                      glm::vec3(-1.0f, 0.5f, 3.0f),
                                      glm::vec3(0.0f));
    glm::mat4 sandWorld   = makeModel(glm::vec3(0.1f),
                                      glm::vec3(0.0f, 0.0f, 1.0f),
                                      glm::vec3(0.0f));
    glm::mat4 chestWorld  = makeModel(glm::vec3(0.2f),
                                      glm::vec3(1.0f, 0.1f, 0.0f),
                                      glm::vec3(0.0f));
    glm::mat4 plantWorld  = makeModel(glm::vec3(0.003f),
                                      glm::vec3(1.0f, 0.1f, 3.0f),
                                      glm::vec3(0.0f));
    glm::mat4 grassWorld  = makeModel(glm::vec3(0.0003f),
                                      glm::vec3(-1.0f, 0.0f, 0.0f),
                                      glm::vec3(0.0f));
    glm::mat4 coralWorld  = makeModel(glm::vec3(0.0005f),
                                      glm::vec3(-1.5f, 0.1f, 1.0f),
                                      glm::vec3(0.0f));

    glm::mat4 pebbleWorld = makeModel(glm::vec3(0.2f),
                                      glm::vec3(-10.5f, -5.0f, 10.5f),
                                      glm::vec3(0.0f));

    glm::mat4 glassPaneWorld  = makeModel(glm::vec3(2.0f,2.0f,0.5f),
                                          glm::vec3(-12.6f, -6.3f, 6.0f),
                                          glm::vec3(0.0f,90.0f,0.0f));
    glm::mat4 glassPane2World = makeModel(glm::vec3(2.0f,2.0f,0.5f),
                                          glm::vec3(7.8f, -6.3f, 6.0f),
                                          glm::vec3(0.0f,90.0f,0.0f));

    glm::vec3 lampPos(-6.0f, 6.2f, 0.0f);
    glm::mat4 lampWorld   = makeModel(glm::vec3(0.07f),
                                      lampPos,
                                      glm::vec3(0.0f));

    glm::mat4 lamp_aWorld = makeModel(glm::vec3(0.03f),
                                      glm::vec3(-1.4f ,-0.15f, -2.7f),
                                      glm::vec3(0.0f,180.0f,0.0f));


    glm::mat4 floorLocal   = glm::inverse(roomWorld) * floorWorld;
    glm::mat4 roofLocal    = glm::inverse(roomWorld) * roofWorld;
    glm::mat4 sandBigLocal = glm::inverse(roomWorld) * sandBigWorld;
    glm::mat4 lampLocal    = glm::inverse(roomWorld) * lampWorld;
    glm::mat4 lamp_aLocal  = glm::inverse(roomWorld) * lamp_aWorld;
    glm::mat4 pebbleLocalM = glm::inverse(roomWorld) * pebbleWorld;

    glm::mat4 tableLocal   = glm::inverse(roomWorld) * tableWorld;
    glm::mat4 boxLocal     = glm::inverse(tableWorld) * boxWorld;
    glm::mat4 glassLocal   = glm::inverse(boxWorld) * glassWorld;
    glm::mat4 sandLocal    = glm::inverse(boxWorld) * sandWorld;
    glm::mat4 chestLocal   = glm::inverse(boxWorld) * chestWorld;
    glm::mat4 plantLocal   = glm::inverse(boxWorld) * plantWorld;
    glm::mat4 grassLocal   = glm::inverse(boxWorld) * grassWorld;
    glm::mat4 coralLocal   = glm::inverse(boxWorld) * coralWorld;

    glm::mat4 glassPaneLocal  = glm::inverse(roomWorld) * glassPaneWorld;
    glm::mat4 glassPane2Local = glm::inverse(roomWorld) * glassPane2World;

    glm::mat4 boxWorldInv  = glm::inverse(boxWorld);

    // --- generovanie kamienkov ---
    const float PEBBLE_Y     = -4.6f;
    const float PEBBLE_MIN_X = -31.35f;
    const float PEBBLE_MAX_X = -17.6f;
    const float PEBBLE_MIN_Z = -13.3f;
    const float PEBBLE_MAX_Z =  20.8f;
    const float PEBBLE_MARGIN = 0.3f;

    std::mt19937 rngPebble(12345);
    std::uniform_real_distribution<float> distPX(PEBBLE_MIN_X + PEBBLE_MARGIN,
                                                 PEBBLE_MAX_X - PEBBLE_MARGIN);
    std::uniform_real_distribution<float> distPZ(PEBBLE_MIN_Z + PEBBLE_MARGIN,
                                                 PEBBLE_MAX_Z - PEBBLE_MARGIN);
    std::uniform_real_distribution<float> distProtY(0.0f, 360.0f);
    std::uniform_real_distribution<float> distPscale(0.05f, 0.12f);

    g_pebbleLocal.clear();
    g_pebbleLocal.reserve(NUM_PEBBLES);

    for (int i = 0; i < NUM_PEBBLES; ++i) {
        glm::vec3 pos(distPX(rngPebble), PEBBLE_Y, distPZ(rngPebble));
        float angleY = glm::radians(distProtY(rngPebble));
        float s      = distPscale(rngPebble);

        glm::mat4 m(1.0f);
        m = glm::translate(m, pos);
        m = glm::rotate(m, angleY, glm::vec3(0,1,0));
        m = glm::scale(m, glm::vec3(s));

        // do room lokálneho priestoru
        glm::mat4 localPebble = glm::inverse(roomWorld) * m;
        g_pebbleLocal.push_back(localPebble);
    }

    // ---- procedurálne ryby a rastliny v akváriu (WORLD space AABB) ----
    std::vector<T_object*> fishVariants  = { &fish, &fish2, &fish3, &fish4, &fish5 };
    std::vector<T_object*> plantVariants = { &plant, &grass, &coral };

    std::mt19937 rngProc(1337);
    std::uniform_real_distribution<float> distFishX(AQUA_MIN_X + AQUA_MARGIN_XZ, AQUA_MAX_X - AQUA_MARGIN_XZ);
    std::uniform_real_distribution<float> distFishY(AQUA_MIN_Y + 0.4f,          AQUA_MAX_Y - 0.4f);
    std::uniform_real_distribution<float> distFishZ(AQUA_MIN_Z + AQUA_MARGIN_XZ, AQUA_MAX_Z - AQUA_MARGIN_XZ);
    std::uniform_real_distribution<float> distVel(-0.8f, 0.8f);
    std::uniform_real_distribution<float> distFishScale(0.0005f, 0.0015f);

    std::uniform_real_distribution<float> distPlantX(AQUA_MIN_X + AQUA_MARGIN_XZ, AQUA_MAX_X - AQUA_MARGIN_XZ);
    std::uniform_real_distribution<float> distPlantZ(AQUA_MIN_Z + AQUA_MARGIN_XZ, AQUA_MAX_Z - AQUA_MARGIN_XZ);
    std::uniform_real_distribution<float> distPlantScale(0.002f, 0.0035f);
    std::uniform_real_distribution<float> distPlantRot(0.0f, 360.0f);

    std::uniform_int_distribution<int> distFishIdx(0, (int)fishVariants.size() - 1);
    std::uniform_int_distribution<int> distPlantIdx(0, (int)plantVariants.size() - 1);

    g_procFish.clear();
    g_procFish.reserve(NUM_PROC_FISH);

    for (int i = 0; i < NUM_PROC_FISH; ++i) {
        ProcFish f;
        f.obj   = fishVariants[distFishIdx(rngProc)];
        f.pos   = glm::vec3(distFishX(rngProc), distFishY(rngProc), distFishZ(rngProc));
        f.vel   = glm::vec3(distVel(rngProc), distVel(rngProc) * 0.4f, distVel(rngProc));
        f.scale = distFishScale(rngProc);

        glm::vec3 dir = glm::length(f.vel) > 0.0001f ? glm::normalize(f.vel) : glm::vec3(0,0,1);
        float yaw = glm::degrees(atan2(dir.x, dir.z));

        glm::mat4 m(1.0f);
        m = glm::translate(m, f.pos);
        m = glm::rotate(m, glm::radians(yaw), glm::vec3(0,1,0));
        m = glm::scale(m, glm::vec3(f.scale));
        f.model = m;

        g_procFish.push_back(f);
    }

    g_procPlants.clear();
    g_procPlants.reserve(NUM_PROC_PLANTS);

    for (int i = 0; i < NUM_PROC_PLANTS; ++i) {
        ProcPlant p;
        p.obj   = plantVariants[distPlantIdx(rngProc)];
        float x = distPlantX(rngProc);
        float z = distPlantZ(rngProc);
        p.scale = distPlantScale(rngProc);
        float rotY = glm::radians(distPlantRot(rngProc));

        glm::mat4 m(1.0f);
        m = glm::translate(m, glm::vec3(x, AQUA_MIN_Y + 0.05f, z)); // na dno akvária
        m = glm::rotate(m, rotY, glm::vec3(0,1,0));
        m = glm::scale(m, glm::vec3(p.scale));
        p.model = m;

        g_procPlants.push_back(p);
    }

    // =====================================
    // Scene graph – uzly
    // =====================================
    SceneNode rootNode { nullptr, glm::mat4(1.0f), {} };

    SceneNode roomNode   { &room, roomWorld, {} };
    SceneNode floorNode  { &floorObj, floorLocal, {} };
    SceneNode roofNode   { &roof,    roofLocal, {} };
    SceneNode sandBigNode{ &sand_big, sandBigLocal, {} };
    SceneNode lampNode   { &lamp, lampLocal, {} };
    SceneNode lamp_aNode { &lamp_a, lamp_aLocal, {} };
    SceneNode pebbleNode { &pebble, pebbleLocalM, {} };

    SceneNode glassPaneNode  { &glass_pane,  glassPaneLocal, {} };
    SceneNode glassPane2Node { &glass_pane2, glassPane2Local, {} };

    SceneNode tableNode  { &table, tableLocal, {} };
    SceneNode boxNode    { &box,   boxLocal, {} };
    SceneNode glassNode  { &glass, glassLocal, {} };
    SceneNode sandNode   { &sand,  sandLocal, {} };
    SceneNode chestNode  { &chest, chestLocal, {} };
    SceneNode plantNode  { &plant, plantLocal, {} };
    SceneNode grassNode  { &grass, grassLocal, {} };
    SceneNode coralNode  { &coral, coralLocal, {} };

    // Dynamické uzly (lokálne matice budeme prepisovať každý frame)
    SceneNode fishNode1 { &fish,  glm::mat4(1.0f), {} };
    SceneNode fishNode2 { &fish2, glm::mat4(1.0f), {} };
    SceneNode fishNode3 { &fish3, glm::mat4(1.0f), {} };
    SceneNode fishNode4 { &fish4, glm::mat4(1.0f), {} };
    SceneNode fishNode5 { &fish5, glm::mat4(1.0f), {} };
    SceneNode diverNode { &diver, glm::mat4(1.0f), {} };

    SceneNode sunNode   { &sunObj, glm::mat4(1.0f), {} };

    // zostavenie stromu
    rootNode.children.push_back(&roomNode);
    rootNode.children.push_back(&sunNode);

    roomNode.children.push_back(&floorNode);
    roomNode.children.push_back(&roofNode);
    roomNode.children.push_back(&sandBigNode);
    roomNode.children.push_back(&lampNode);
    roomNode.children.push_back(&lamp_aNode);
    roomNode.children.push_back(&glassPaneNode);
    roomNode.children.push_back(&glassPane2Node);
    roomNode.children.push_back(&tableNode);

    tableNode.children.push_back(&boxNode);

    boxNode.children.push_back(&glassNode);
    boxNode.children.push_back(&sandNode);
    boxNode.children.push_back(&chestNode);
    boxNode.children.push_back(&plantNode);
    boxNode.children.push_back(&grassNode);
    boxNode.children.push_back(&coralNode);
    boxNode.children.push_back(&fishNode1);
    boxNode.children.push_back(&fishNode2);
    boxNode.children.push_back(&fishNode3);
    boxNode.children.push_back(&fishNode4);
    boxNode.children.push_back(&fishNode5);
    boxNode.children.push_back(&diverNode);

    // ======================
    // Hlavný loop
    // ======================
    while(!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float dt = currentFrame - lastFrameTime;
        lastFrameTime = currentFrame;

        float elapsedTime = currentFrame - startTime;

        // =========================================
        // P – prepínač postprocessu
        // =========================================
        {
            static bool pWasPressed = false;
            bool pNow = (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS);
            if (pNow && !pWasPressed) {
                g_postEnabled = !g_postEnabled;
                std::cout << "Post-process: "
                          << (g_postEnabled ? "ON" : "OFF") << std::endl;
            }
            pWasPressed = pNow;
        }

        // Blur aktívny len v danom časovom intervale
        bool blurActive = g_postEnabled && ((elapsedTime >= 62.0f && elapsedTime < 107.0f) || (elapsedTime >= 144.5f && elapsedTime < 173.0f));

        // =========================================
        // B – prepínanie Phong / Blinn-Phong
        // =========================================
        {
            static bool bWasPressed = false;
            bool bNow = (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS);
            if (bNow && !bWasPressed) {
                g_useBlinn = !g_useBlinn;
                std::cout << "Shading model: "
                          << (g_useBlinn ? "Blinn-Phong" : "Phong")
                          << std::endl;
            }
            bWasPressed = bNow;
        }

        // L – spustenie/cyklus Slnko/Mesiac
        {
            static bool lWasPressed = false;
            bool lNow = (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS);
            if (lNow && !lWasPressed) {
                if (!sunAnimating) {
                    sunAnimating = true;
                }
            }
            lWasPressed = lNow;
        }

        // K – zapnutie/vypnutie logovania pozície kamery
        static bool kLast = false;
        bool kNow = (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS);
        if (kNow && !kLast) {
            g_printCoords = !g_printCoords;
            std::cout << "Coordinate logging: " << (g_printCoords ? "ON" : "OFF") << std::endl;
        }
        kLast = kNow;

        // G – prepínač kamery (keyframe / free)
        {
            static bool g_was_pressed = false;
            bool g_now = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
            if (g_now && !g_was_pressed) {
                useKeyframeCamera = !useKeyframeCamera;
                std::cout << "Camera mode: " << (useKeyframeCamera ? "KEYFRAME" : "FREE") << std::endl;
            }
            g_was_pressed = g_now;
        }

        processInput(window, dt);

        // =========================================
        // Slnko / Mesiac update
        // =========================================
        if (sunAnimating)
        {
            float speed = SUN_BASE_SPEED;

            /*if (sunParam >= 0.9f && sunParam <= 1.1f)
                speed *= 100.0f;*/

            if (sunCycleState == 0)
            {
                sunParam += speed * dt;

                if (sunParam >= 1.5f)
                {
                    sunParam = 1.5f;
                    sunAnimating = false;
                    sunCycleState = 1;
                }
            }
            else if (sunCycleState == 1)
            {
                float prev = sunParam;
                sunParam += speed * dt;

                if (sunParam >= 2.0f)
                    sunParam -= 2.0f;

                if (prev < 0.5f && sunParam >= 0.5f)
                {
                    sunParam = 0.5f;
                    sunAnimating = false;
                    sunCycleState = 0;
                }
            }
        }

        bool isMoon = (sunParam >= 1.0f);

        // =========================================
        // Ryba 5 – keyframe animácia
        // =========================================
        fish5CurrentTime += dt;
        updateFish5Animation(fish5CurrentTime, fish5Keyframes, fish5Pos, fish5Rot);

        // diver – fyzika
        diverBody.applyForce(glm::vec3(0, -0.3f, 0));
        diverBody.update(dt);
        diverBody.checkFloorCollision();

        // =========================================
        // Animácie pevne daných rýb
        // =========================================
        glm::vec3 fishPos, fishRot;
        glm::vec3 fish4Rot;
        {
            float speed = 0.2f * dt;
            glm::vec3 startPos = glm::vec3(0.6f, 1.0f, 0.5f);
            glm::vec3 endPos   = glm::vec3(-0.6f, 1.0f, 0.5f);

            if (forwardFish1) tFish1 += speed;
            else              tFish1 -= speed;

            if (tFish1 > 1.0f) { tFish1 = 1.0f; forwardFish1 = false; }
            if (tFish1 < 0.0f) { tFish1 = 0.0f; forwardFish1 = true; }

            fishPos = glm::mix(startPos, endPos, tFish1);
            fishPos.y += sin(glfwGetTime() * 2.0f) * 0.05f;
            fishPos.z += sin(glfwGetTime() * 6.0f) * 0.03f;

            glm::vec3 direction = glm::normalize(endPos - startPos);
            if (!forwardFish1) direction = -direction;

            float fishYaw = glm::degrees(atan2(direction.x, direction.z));
            float baseYaw = 100.0f;
            float facingOffset = forwardFish1 ? 0.0f : 180.0f;
            float facez = forwardFish1 ? 0.0f : 180.0f;
            fishRot = glm::vec3(-90.0f, fishYaw+baseYaw+facingOffset, facez);

            const float xMin = -1.7f, xMax = 1.7f;
            const float yMin = 1.0f, yMax = 2.3f;
            const float zMin = -0.8f, zMax = 3.6f;

            fish4Pos += fish4Vel * dt;

            if (fish4Pos.x < xMin) { fish4Pos.x = xMin; fish4Vel.x *= -1.0f; }
            if (fish4Pos.x > xMax) { fish4Pos.x = xMax; fish4Vel.x *= -1.0f; }

            if (fish4Pos.y < yMin) { fish4Pos.y = yMin; fish4Vel.y *= -1.0f; }
            if (fish4Pos.y > yMax) { fish4Pos.y = yMax; fish4Vel.y *= -1.0f; }

            if (fish4Pos.z < zMin) { fish4Pos.z = zMin; fish4Vel.z *= -1.0f; }
            if (fish4Pos.z > zMax) { fish4Pos.z = zMax; fish4Vel.z *= -1.0f; }

            glm::vec3 dir4 = glm::normalize(fish4Vel);
            float yaw4 = glm::degrees(atan2(dir4.x, dir4.z));
            fish4Rot = glm::vec3(0.0f, yaw4, 0.0f);
        }

        glm::vec3 fish2Pos = glm::vec3(1.0f, 0.1f, 1.5f);
        glm::vec3 fish2Rot = glm::vec3(20.0f,
                                       20.0f + sin(glfwGetTime() * 4.0f) * 5.0f,
                                       0.0f);

        tFish3 += dt * 0.2f;
        if (tFish3 >= 1.0f) tFish3 -= 1.0f;

        int count = (int)fish3Points.size();
        float scaledT = tFish3 * count;
        int seg = (int)scaledT;
        float localT = scaledT - seg;
        int i0 = (seg - 1 + count) % count;
        int i1 = (seg     ) % count;
        int i2 = (seg + 1 ) % count;
        int i3 = (seg + 2 ) % count;

        glm::vec3 fish3Pos = CatmullRom(
                fish3Points[i0],
                fish3Points[i1],
                fish3Points[i2],
                fish3Points[i3],
                localT
        );

        glm::vec3 nextPos = CatmullRom(
                fish3Points[i0],
                fish3Points[i1],
                fish3Points[i2],
                fish3Points[i3],
                localT + 0.01f
        );

        glm::vec3 dir3 = glm::normalize(nextPos - fish3Pos);
        float yaw3 = glm::degrees(atan2(dir3.x, dir3.z));
        glm::vec3 fish3RotVec = glm::vec3(0.0f, yaw3, 0.0f);

        // =========================================
        // Update procedurálnych rýb v reálnom akváriu (WORLD AABB)
        // =========================================
        for (auto &f : g_procFish) {
            f.pos += f.vel * dt;

            if (f.pos.x < AQUA_MIN_X + AQUA_MARGIN_XZ) { f.pos.x = AQUA_MIN_X + AQUA_MARGIN_XZ; f.vel.x *= -1.0f; }
            if (f.pos.x > AQUA_MAX_X - AQUA_MARGIN_XZ) { f.pos.x = AQUA_MAX_X - AQUA_MARGIN_XZ; f.vel.x *= -1.0f; }

            if (f.pos.y < AQUA_MIN_Y + 0.2f) { f.pos.y = AQUA_MIN_Y + 0.2f; f.vel.y *= -1.0f; }
            if (f.pos.y > AQUA_MAX_Y - 0.2f) { f.pos.y = AQUA_MAX_Y - 0.2f; f.vel.y *= -1.0f; }

            if (f.pos.z < AQUA_MIN_Z + AQUA_MARGIN_XZ) { f.pos.z = AQUA_MIN_Z + AQUA_MARGIN_XZ; f.vel.z *= -1.0f; }
            if (f.pos.z > AQUA_MAX_Z - AQUA_MARGIN_XZ) { f.pos.z = AQUA_MAX_Z - AQUA_MARGIN_XZ; f.vel.z *= -1.0f; }

            glm::vec3 dir = glm::length(f.vel) > 0.0001f ? glm::normalize(f.vel) : glm::vec3(0,0,1);
            float yaw = glm::degrees(atan2(dir.x, dir.z));

            glm::mat4 m(1.0f);
            m = glm::translate(m, f.pos);
            m = glm::rotate(m, glm::radians(yaw), glm::vec3(0,1,0));
            m = glm::scale(m, glm::vec3(f.scale));
            f.model = m;
        }

        // =========================================
        // Svetlá – dynamická pozícia Slnka/Mesiaca
        // =========================================
        glm::vec3 sunPos = computeSunPosFromParam(sunParam);

        glm::vec3 sunColor  = isMoon
                              ? glm::vec3(0.40f, 0.50f, 0.80f)
                              : glm::vec3(1.00f, 0.95f, 0.80f);

        // SPOTLIGHT
        glm::vec3 spotPos(-2.0f, 3.5f, -3.5f);

        // Slnko
        float near_plane = 1.0f, far_plane = 200.0f;
        glm::mat4 sunProj = glm::ortho(-50.0f, 50.0f,
                                       -50.0f, 50.0f,
                                       near_plane, far_plane);
        glm::mat4 sunView = glm::lookAt(sunPos,
                                        glm::vec3(0.0f, 0.0f, 0.0f),
                                        glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 sunLightSpace = sunProj * sunView;

        // Lampa
        glm::mat4 lampProj = glm::ortho(-25.0f, 25.0f,
                                        -25.0f, 25.0f,
                                        1.5f, 25.0f);
        glm::mat4 lampView = glm::lookAt(lampPos,
                                         glm::vec3(0.0f, 0.0f, 0.0f),
                                         glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lampLightSpace = lampProj * lampView;

        // aktualizácia dynamických uzlov v grafe (lokálne voči boxWorld)
        glm::mat4 fishWorld1 = makeModel(glm::vec3(0.01f), fishPos, fishRot);
        glm::mat4 fishWorld2 = makeModel(glm::vec3(0.001f), fish2Pos, fish2Rot);
        glm::mat4 fishWorld3 = makeModel(glm::vec3(0.0008f), fish3Pos, fish3RotVec);
        glm::mat4 fishWorld4 = makeModel(glm::vec3(0.001f), fish4Pos, fish4Rot);
        glm::mat4 fishWorld5 = makeModel(glm::vec3(0.2f),   fish5Pos, fish5Rot);
        glm::mat4 diverWorld = makeModel(glm::vec3(0.005f), diverBody.position, diverBody.rotation);

        fishNode1.localTransform = boxWorldInv * fishWorld1;
        fishNode2.localTransform = boxWorldInv * fishWorld2;
        fishNode3.localTransform = boxWorldInv * fishWorld3;
        fishNode4.localTransform = boxWorldInv * fishWorld4;
        fishNode5.localTransform = boxWorldInv * fishWorld5;
        diverNode.localTransform = boxWorldInv * diverWorld;

        auto isTransparentDepth = [&](const T_object* obj) {
            return (obj == &glass ||
                    obj == &box   ||
                    obj == &glass_pane ||
                    obj == &glass_pane2);
        };

        // =========================================
        // 1) Shadow pass – Slnko
        // =========================================
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, sunDepthFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT);

        drawSceneNodeDepth(&rootNode, glm::mat4(1.0f), depthShader, sunLightSpace, isTransparentDepth);

        // procedurálne ryby + rastliny
        for (const auto &f : g_procFish) {
            if (f.obj) f.obj->renderDepthWithModel(depthShader, sunLightSpace, f.model);
        }
        for (const auto &p : g_procPlants) {
            if (p.obj) p.obj->renderDepthWithModel(depthShader, sunLightSpace, p.model);
        }

        glCullFace(GL_BACK);

        // =========================================
        // 2) Shadow pass – lampa
        // =========================================
        glBindFramebuffer(GL_FRAMEBUFFER, lampDepthFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT);

        drawSceneNodeDepth(&rootNode, glm::mat4(1.0f), depthShader, lampLightSpace, isTransparentDepth);

        for (const auto &f : g_procFish) {
            if (f.obj) f.obj->renderDepthWithModel(depthShader, lampLightSpace, f.model);
        }
        for (const auto &p : g_procPlants) {
            if (p.obj) p.obj->renderDepthWithModel(depthShader, lampLightSpace, p.model);
        }

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // =========================================
        // 3) Normálny pass – render do HDR FBO
        // =========================================
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0,0,0,1);
        postProcess.BindFramebuffer();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                (float)SCR_WIDTH / (float)SCR_HEIGHT,
                0.1f, 100.0f
        );

        camCurrentTime += dt;

        glm::mat4 view;
        glm::vec3 activeCamPos;

        if (useKeyframeCamera) {
            glm::vec3 camPos    = interpolateCatmullRom(camKeyframes, camCurrentTime, true);
            glm::vec3 camLookAt = interpolateCatmullRom(camKeyframes, camCurrentTime, false);

            glm::vec3 front = glm::normalize(camLookAt - camPos);
            glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0,1,0)));
            glm::vec3 up    = glm::normalize(glm::cross(right, front));

            view = glm::lookAt(camPos, camPos + front, up);
            activeCamPos = camPos;
        } else {
            view = camera.getViewMatrix();
            activeCamPos = camera.getPosition();
        }

        // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        // SKYBOX – pozadie v HDR FBO
        // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        RenderSkybox(view, projection);

        std::vector<glm::vec3> lightPositions = {
                sunPos,
                lampPos,
                spotPos
        };
        std::vector<glm::vec3> lightColors    = {
                sunColor,
                glm::vec3(1.f, 0.95f, 0.8f),
                glm::vec3(0.7f, 0.8f, 1.0f)*3.0f
        };

        T_object::setLights(lightPositions, lightColors, activeCamPos);
        T_object::setLightSpaceMatrices(sunLightSpace, lampLightSpace);
        T_object::setShadowMaps(sunDepthMap, true,lampDepthMap, true);
        T_object::setUseBlinn(g_useBlinn);

        // -------------------------------
        // OPAQUE + TRANSPARENT render
        // -------------------------------
        auto isTransparentNode = [&](SceneNode* n) {
            if (!n->object) return false;
            return (n->object == &glass ||
                    n->object == &box   ||
                    n->object == &glass_pane ||
                    n->object == &glass_pane2);
        };

        std::function<void(SceneNode*, const glm::mat4&, bool)> drawSelective;
        drawSelective = [&](SceneNode* node,
                            const glm::mat4& parent,
                            bool drawTransparent)
        {
            glm::mat4 model = parent * node->localTransform;

            bool isTransp = isTransparentNode(node);

            if (node->object && (isTransp == drawTransparent)) {
                node->object->renderWithModel(view, projection, model);
            }

            for (SceneNode* child : node->children) {
                drawSelective(child, model, drawTransparent);
            }
        };

        // 1) NEPRIEHĽADNÉ objekty
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        drawSelective(&rootNode, glm::mat4(1.0f), false);

        // všetky inštancie kamienkov
        for (const glm::mat4& localPebble : g_pebbleLocal) {
            glm::mat4 model = roomWorld * localPebble;
            pebble.renderWithModel(view, projection, model);
        }

        // procedurálne ryby
        for (const auto &f : g_procFish) {
            if (f.obj) f.obj->renderWithModel(view, projection, f.model);
        }

        // procedurálne rastliny
        for (const auto &p : g_procPlants) {
            if (p.obj) p.obj->renderWithModel(view, projection, p.model);
        }

        // 2) PRIEHĽADNÉ objekty
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        drawSelective(&rootNode, glm::mat4(1.0f), true);
        glDepthMask(GL_TRUE);

        // Particles – bublinky z truhlice v akváriu
        particleSystem.setEmitterPosition(glm::vec3(1.05f, 0.0f, 1.7f));
        particleSystem.setFishCollider(glm::vec3(fish2Pos.x,fish2Pos.y+0.03f,fish2Pos.z+0.02f),
                                       glm::vec3(0.05f, 0.05f, 0.2f),
                                       fish2Rot);
        particleSystem.update(dt);
        particleSystem.render(view, projection);
        //particleSystem.renderCollider(view, projection);

        postProcess.UnbindFramebuffer();

        // =========================================
        // 4) BLUR PASS – z HDR scény do gBlurColorTex
        // =========================================
        if (g_postEnabled) {
            glBindFramebuffer(GL_FRAMEBUFFER, gBlurFBO);
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
            glClear(GL_COLOR_BUFFER_BIT);

            glDisable(GL_DEPTH_TEST);

            glUseProgram(gBlurProgram);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, postProcess.getColorTexture());
            glUniform1i(glGetUniformLocation(gBlurProgram, "screenTexture"), 0);

            glBindVertexArray(gBlurVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // =========================================
        // 5) FINÁLNY COMPOSE – HDR + bloom (+ blur)
        // =========================================
        if (g_postEnabled)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            postProcess.Render(
                    blurActive,
                    gBlurColorTex
            );
        }
        else
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, postProcess.getFBO());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT,
                              0, 0, SCR_WIDTH, SCR_HEIGHT,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        // =========================================
        // Voliteľný výpis pozície kamery (K)
        // =========================================
        if (g_printCoords) {
            static float coordTimer = 0.0f;
            coordTimer += dt;

            if (coordTimer >= 0.1f) {
                coordTimer = 0.0f;

                glm::vec3 pos;
                if (useKeyframeCamera) {
                    pos = activeCamPos;
                } else {
                    pos = camera.getPosition();
                }

                std::cout << "Cam pos: "
                          << pos.x << ", "
                          << pos.y << ", "
                          << pos.z << std::endl;
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
