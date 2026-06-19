#pragma once
#include <iostream>
#include <vector>
#include <fstream>
#include <memory>

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "texture.h"

// Edit by: Samuel Zaprazny
// Adding assimp library
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace ppgso {

    class Mesh_Assimp {
        struct gl_buffer {
        public:
            GLuint vao, vbo, tbo, nbo, ibo = 0;
            GLsizei size = 0;
        };

        std::vector<gl_buffer> buffers;
        const aiScene * scene;

        // Loaded materials
        std::vector<glm::vec3> ambient;
        std::vector<glm::vec3> diffuse;
        std::vector<glm::vec3> specular;

    public:

        /*!
         * Load 3D geometry from a na Wavefront .obj file.
         *
         * The shader program passed to the object will be bound to the geometry as follows:
         * vec3 Position - Vertex position, position 0
         * vec2 TexCoord - Texture coordinate, position 1
         * vec3 Normal - Normal vector, position 2
         *
         * @param obj - File path to the obj file to load.
         */
        Mesh_Assimp(const std::string &obj);

        ~Mesh_Assimp();

        void processNode(aiNode *node, const aiScene *pScene);

        void processMesh(aiMesh *mesh);

        /*!
         * Render the geometry associated with the mesh using glDrawElements.
         */
        void render();
    };
}

