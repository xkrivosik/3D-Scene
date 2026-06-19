#include "t_object.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// =====================================
// Statické členy
// =====================================
glm::vec3 T_object::s_lightPositions[T_object::MAX_LIGHTS];
glm::vec3 T_object::s_lightColors[T_object::MAX_LIGHTS];
int       T_object::s_lightCount = 0;
glm::vec3 T_object::s_viewPos(0.0f);

glm::mat4 T_object::s_lightSpace0(1.0f);
glm::mat4 T_object::s_lightSpace1(1.0f);

GLuint T_object::s_shadowMap0 = 0;
GLuint T_object::s_shadowMap1 = 0;
bool   T_object::s_useShadow0 = false;
bool   T_object::s_useShadow1 = false;
bool   T_object::s_useBlinn   = true;

// =====================================
// Shadery – source
// =====================================

static const char* vsSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 uLightSpace0;
uniform mat4 uLightSpace1;
uniform vec2 uTexScale;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLightSpace0;
out vec4 FragPosLightSpace1;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal  = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord * uTexScale;

    FragPosLightSpace0 = uLightSpace0 * worldPos;
    FragPosLightSpace1 = uLightSpace1 * worldPos;

    gl_Position = projection * view * worldPos;
}
)";

static const char* fsTextured = R"(
#version 330 core

struct Light {
    vec3 position;
    vec3 color;
    float enabled;
};

const int MAX_LIGHTS = 4;
const int SPOT_INDEX = 2;

const vec3  spotDirection   = normalize(vec3(0.0, -2.5, 0.5));
const float spotInnerCutOff = cos(radians(1.0));
const float spotOuterCutOff = cos(radians(10.5));

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace0;
in vec4 FragPosLightSpace1;

out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D uShadowMap0;
uniform sampler2D uShadowMap1;

// normal mapa
uniform sampler2D uNormalMap;
uniform bool      uUseNormalMap;

uniform bool  uUseShadow0;
uniform bool  uUseShadow1;

uniform int   uLightCount;
uniform Light uLights[MAX_LIGHTS];

uniform vec3  uViewPos;
uniform float uSpecularStrength;
uniform float uShininess;
uniform bool  uUseBlinn;

// ----------------------------------------------------
// výpočet tieňa z jednej shadow mapy
// ----------------------------------------------------
float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    float bias = 0.005;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    for(int y = -1; y <= 1; ++y)
    {
        float pcfDepth = texture(shadowMap,
                                 projCoords.xy + vec2(x, y) * texelSize).r;
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
    }
    shadow /= 9.0;

    return shadow;
}

// ----------------------------------------------------
// Normála – buď z geometrie, alebo z normal mapy
// ----------------------------------------------------
vec3 getNormal(vec3 geomNormal, vec2 uv, sampler2D normalMap, bool useMap)
{
    vec3 baseN = normalize(geomNormal);

    if (!useMap) {
        return baseN;
    }

    // normal map v tangent space
    vec3 mapN = texture(normalMap, uv).rgb;
    mapN = mapN * 2.0 - 1.0; // [0,1] -> [-1,1]

    // Pre rovný plát v XZ rovine (Y hore):
    vec3 N_geom = vec3(0.0, 1.0, 0.0);
    vec3 T      = vec3(1.0, 0.0, 0.0);
    vec3 B      = vec3(0.0, 0.0, 1.0);

    mat3 TBN = mat3(T, B, N_geom);
    vec3 N_final = normalize(TBN * mapN);

    return N_final;
}

//svetla

void main()
{
    vec4 texColor = texture(texture1, TexCoord);
    if (texColor.a < 0.05)
        discard;

    vec3 albedo = texColor.rgb;
    float alpha = texColor.a;

    vec3 norm    = getNormal(Normal, TexCoord, uNormalMap, uUseNormalMap);
    vec3 viewDir = normalize(uViewPos - FragPos);

    vec3 ambient = 0.1 * albedo;
    vec3 result  = ambient;

    for (int i = 0; i < uLightCount; ++i) {
        if (uLights[i].enabled < 0.5)
            continue;

        vec3 lightDir = normalize(uLights[i].position - FragPos);
        float diff    = max(dot(norm, lightDir), 0.0);

        float spec;
        if (uUseBlinn) {
            vec3 halfwayDir = normalize(lightDir + viewDir);
            spec = pow(max(dot(norm, halfwayDir), 0.0), uShininess);
        } else {
            vec3 reflectDir = reflect(-lightDir, norm);
            spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
        }

        float spotFactor = 1.0;
        float attenuation = 1.0;

        // Point
        if (i >= 1) {
            float dist = length(uLights[i].position - FragPos);
            float constant  = 1.0;
            float linear    = 0.09;
            float quadratic = 0.0002;
            attenuation = 1.0 / (constant + linear * dist + quadratic * dist * dist);
        }

        // Spotlight
        if (i == SPOT_INDEX) {
            vec3 L = normalize(FragPos - uLights[i].position);
            float theta = dot(L, spotDirection);
            if (theta > spotOuterCutOff) {
                float epsilon   = spotInnerCutOff - spotOuterCutOff;
                float intensity = clamp((theta - spotOuterCutOff) / epsilon, 0.0, 1.0);
                spotFactor = intensity;
            } else {
                spotFactor = 0.0;
            }

            if (FragPos.y < -1.0)
                spotFactor = 0.0;
        }

        vec3 diffuse  = diff * albedo * uLights[i].color * spotFactor * attenuation;
        vec3 specular = uSpecularStrength * spec * uLights[i].color * spotFactor * attenuation;

        float shadowFactor = 0.0;
        if (i == 0 && uUseShadow0) {
            shadowFactor = ShadowCalculation(FragPosLightSpace0, uShadowMap0);
        } else if (i == 1 && uUseShadow1) {
            shadowFactor = ShadowCalculation(FragPosLightSpace1, uShadowMap1);
        }

        result += (1.0 - shadowFactor) * (diffuse + specular);
    }

    FragColor = vec4(result, alpha);
}
)";

static const char* fsGlass = R"(
#version 330 core
out vec4 FragColor;

void main()
{
    if (!gl_FrontFacing){
        discard;
    }
    FragColor = vec4(0.78, 0.88, 0.96, 0.33);
}
)";

// =====================================
// BMP loader (24-bit nekomprimované BMP)
// =====================================
GLuint T_object::loadBMP(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "BMP missing: " << path << "\n";
        return 0;
    }

    unsigned char header[54];
    file.read(reinterpret_cast<char*>(header), 54);
    if (file.gcount() != 54) {
        std::cerr << "BMP header too small: " << path << "\n";
        return 0;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        std::cerr << "Not a BMP file: " << path << "\n";
        return 0;
    }

    unsigned int dataPos   = *reinterpret_cast<int*>(&header[0x0A]);
    unsigned int width     = *reinterpret_cast<int*>(&header[0x12]);
    unsigned int height    = *reinterpret_cast<int*>(&header[0x16]);
    unsigned int imageSize = *reinterpret_cast<int*>(&header[0x22]);

    if (imageSize == 0) imageSize = width * height * 3;
    if (dataPos   == 0) dataPos   = 54;

    std::vector<unsigned char> data(imageSize);
    file.seekg(dataPos, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), imageSize);
    file.close();

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 width,
                 height,
                 0,
                 GL_BGR,
                 GL_UNSIGNED_BYTE,
                 data.data());

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return tex;
}

// =====================================
// OBJ loader
// =====================================
void T_object::loadOBJ(const char* path)
{
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_uvs;
    std::vector<glm::vec3> temp_normals;

    std::vector<unsigned int> vertexIndices;
    std::vector<unsigned int> uvIndices;
    std::vector<unsigned int> normalIndices;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Cannot open OBJ: " << path << "\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            glm::vec3 v;
            ss >> v.x >> v.y >> v.z;
            temp_vertices.push_back(v);
        }
        else if (prefix == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            temp_uvs.push_back(uv);
        }
        else if (prefix == "vn") {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            temp_normals.push_back(n);
        }
        else if (prefix == "f") {
            std::string vertex;
            std::vector<std::string> faceVertices;

            while (ss >> vertex) {
                faceVertices.push_back(vertex);
            }

            for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                const std::string& v1 = faceVertices[0];
                const std::string& v2 = faceVertices[i];
                const std::string& v3 = faceVertices[i + 1];

                unsigned int vi[3] = {0}, ti[3] = {0}, ni[3] = {0};

                std::sscanf(v1.c_str(), "%u/%u/%u", &vi[0], &ti[0], &ni[0]);
                std::sscanf(v2.c_str(), "%u/%u/%u", &vi[1], &ti[1], &ni[1]);
                std::sscanf(v3.c_str(), "%u/%u/%u", &vi[2], &ti[2], &ni[2]);

                for (int j = 0; j < 3; ++j) {
                    vertexIndices.push_back(vi[j] - 1);
                    uvIndices.push_back(ti[j] ? ti[j] - 1 : 0);
                    normalIndices.push_back(ni[j] ? ni[j] - 1 : 0);
                }
            }
        }
    }

    file.close();

    std::vector<float> vertexData;
    vertexData.reserve(vertexIndices.size() * 8);

    for (size_t i = 0; i < vertexIndices.size(); ++i) {
        unsigned int vi = vertexIndices[i];
        unsigned int ti = (i < uvIndices.size()) ? uvIndices[i] : 0;
        unsigned int ni = (i < normalIndices.size()) ? normalIndices[i] : 0;

        glm::vec3 pos    = temp_vertices[vi];
        glm::vec2 uv     = (ti < temp_uvs.size())     ? temp_uvs[ti]     : glm::vec2(0.0f);
        glm::vec3 normal = (ni < temp_normals.size()) ? temp_normals[ni] : glm::vec3(0,1,0);

        vertexData.push_back(pos.x);
        vertexData.push_back(pos.y);
        vertexData.push_back(pos.z);
        vertexData.push_back(uv.x);
        vertexData.push_back(uv.y);
        vertexData.push_back(normal.x);
        vertexData.push_back(normal.y);
        vertexData.push_back(normal.z);
    }

    vertexCount = static_cast<GLuint>(vertexIndices.size());

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vertexData.size() * sizeof(float),
                 vertexData.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          8 * sizeof(float),
                          (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          8 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          8 * sizeof(float),
                          (void*)(5 * sizeof(float)));
}

// =====================================
// Shadery – vytvorenie
// =====================================
void T_object::createTexturedShader()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsTextured, nullptr);
    glCompileShader(fs);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);
}

void T_object::createGlassShader()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsGlass, nullptr);
    glCompileShader(fs);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);
}

// =====================================
// Konštruktory
// =====================================
T_object::T_object(const char* objPath, const char* texPath)
{
    createTexturedShader();
    loadOBJ(objPath);
    textureID = loadBMP(texPath);

    if (!textureID) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        unsigned char white[] = { 255, 255, 255, 255 };
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     1,
                     1,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    isGlass      = false;
    normalTex    = 0;
    hasNormalMap = false;
}

T_object::T_object(const char* objPath, bool makeItGlass)
{
    createGlassShader();
    loadOBJ(objPath);

    textureID    = 0;
    normalTex    = 0;
    hasNormalMap = false;
    isGlass      = true;
}

// Normal mapa
void T_object::setNormalMap(const char* normalPath)
{
    if (isGlass) {
        hasNormalMap = false;
        return;
    }

    normalTex = loadBMP(normalPath);
    if (normalTex) {
        hasNormalMap = true;
    } else {
        hasNormalMap = false;
    }
}

// =====================================
// Statické nastavenia (svetlá & shadow mapy)
// =====================================
void T_object::setLights(const std::vector<glm::vec3>& positions,
                         const std::vector<glm::vec3>& colors,
                         const glm::vec3& viewPos)
{
    s_lightCount = (int)std::min(positions.size(), (size_t)MAX_LIGHTS);
    for (int i = 0; i < s_lightCount; ++i) {
        s_lightPositions[i] = positions[i];
        s_lightColors[i]    = (i < (int)colors.size()) ? colors[i] : glm::vec3(1.0f);
    }
    s_viewPos = viewPos;
}

void T_object::setLightSpaceMatrices(const glm::mat4& lightSpace0,
                                     const glm::mat4& lightSpace1)
{
    s_lightSpace0 = lightSpace0;
    s_lightSpace1 = lightSpace1;
}

void T_object::setShadowMaps(GLuint shadow0, bool use0,
                             GLuint shadow1, bool use1)
{
    s_shadowMap0 = shadow0;
    s_shadowMap1 = shadow1;
    s_useShadow0 = use0;
    s_useShadow1 = use1;
}

void T_object::setUseBlinn(bool enable)
{
    s_useBlinn = enable;
}

// =====================================
// Render
// =====================================
void T_object::render(const glm::mat4& view,
                      const glm::mat4& projection,
                      const glm::vec3& scale,
                      const glm::vec3& pos,
                      const glm::vec3& rot)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
    m = glm::rotate(m, glm::radians(rot.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rot.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rot.z), glm::vec3(0, 0, 1));
    m = glm::scale(m, scale);

    renderWithModel(view, projection, m);
}

void T_object::renderWithModel(const glm::mat4& view,
                               const glm::mat4& projection,
                               const glm::mat4& model)
{
    glUseProgram(shaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),
                       1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"),
                       1, GL_FALSE, glm::value_ptr(projection));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uLightSpace0"),
                       1, GL_FALSE, glm::value_ptr(s_lightSpace0));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uLightSpace1"),
                       1, GL_FALSE, glm::value_ptr(s_lightSpace1));

    GLint texScaleLoc = glGetUniformLocation(shaderProgram, "uTexScale");
    if (texScaleLoc != -1) {
        glUniform2fv(texScaleLoc, 1, &texScale[0]);
    }

    if (!isGlass && textureID) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    }

    if (!isGlass) {
        glUniform1i(glGetUniformLocation(shaderProgram, "uLightCount"),
                    s_lightCount);

        for (int i = 0; i < s_lightCount; ++i) {
            std::string base = "uLights[" + std::to_string(i) + "].";
            glUniform3fv(glGetUniformLocation(shaderProgram,
                                              (base + "position").c_str()),
                         1, glm::value_ptr(s_lightPositions[i]));
            glUniform3fv(glGetUniformLocation(shaderProgram,
                                              (base + "color").c_str()),
                         1, glm::value_ptr(s_lightColors[i]));
            glUniform1f(glGetUniformLocation(shaderProgram,
                                             (base + "enabled").c_str()),
                        1.0f);
        }

        glUniform3fv(glGetUniformLocation(shaderProgram, "uViewPos"),
                     1, glm::value_ptr(s_viewPos));

        glUniform1f(glGetUniformLocation(shaderProgram, "uSpecularStrength"),
                    specularStrength);
        glUniform1f(glGetUniformLocation(shaderProgram, "uShininess"),
                    shininess);

        glUniform1i(glGetUniformLocation(shaderProgram, "uUseBlinn"),
                    s_useBlinn ? 1 : 0);

        // Shadow mapy
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_shadowMap0);
        glUniform1i(glGetUniformLocation(shaderProgram, "uShadowMap0"), 1);
        glUniform1i(glGetUniformLocation(shaderProgram, "uUseShadow0"),
                    s_useShadow0 ? 1 : 0);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, s_shadowMap1);
        glUniform1i(glGetUniformLocation(shaderProgram, "uShadowMap1"), 2);
        glUniform1i(glGetUniformLocation(shaderProgram, "uUseShadow1"),
                    s_useShadow1 ? 1 : 0);

        // Normal mapa
        GLint useNormalLoc = glGetUniformLocation(shaderProgram, "uUseNormalMap");
        if (useNormalLoc != -1) {
            glUniform1i(useNormalLoc, hasNormalMap ? 1 : 0);
        }

        if (hasNormalMap && normalTex) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, normalTex);
            glUniform1i(glGetUniformLocation(shaderProgram, "uNormalMap"), 3);
        }
    }

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

// =====================================
// Render do depth mapy
// =====================================
void T_object::renderDepth(GLuint depthShader,
                           const glm::mat4& lightSpaceMatrix,
                           const glm::vec3& scale,
                           const glm::vec3& pos,
                           const glm::vec3& rot)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
    m = glm::rotate(m, glm::radians(rot.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rot.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rot.z), glm::vec3(0, 0, 1));
    m = glm::scale(m, scale);

    renderDepthWithModel(depthShader, lightSpaceMatrix, m);
}

void T_object::renderDepthWithModel(GLuint depthShader,
                                    const glm::mat4& lightSpaceMatrix,
                                    const glm::mat4& model)
{
    glUseProgram(depthShader);

    glUniformMatrix4fv(glGetUniformLocation(depthShader, "model"),
                       1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(depthShader, "lightSpaceMatrix"),
                       1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

// =====================================
// Destruktor
// =====================================
T_object::~T_object()
{
    if (textureID)    glDeleteTextures(1, &textureID);
    if (normalTex)    glDeleteTextures(1, &normalTex);
    if (VBO)          glDeleteBuffers(1, &VBO);
    if (VAO)          glDeleteVertexArrays(1, &VAO);
    if (shaderProgram)glDeleteProgram(shaderProgram);
}
