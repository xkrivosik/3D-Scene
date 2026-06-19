#include <glm/glm.hpp>
#include <sstream>

#include "Mesh_Assimp.h"

ppgso::Mesh_Assimp::Mesh_Assimp(const std::string &obj_file) {
#ifdef DEBBUG_MODE
    std::cout << "Using ASSIMP Loader!" << std::endl;
#endif

    Assimp::Importer importer;
    scene = importer.ReadFile(obj_file, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::stringstream msg;
        msg << importer.GetErrorString() << std::endl << "Failed to load OBJ file " << obj_file << "!" << std::endl;
        throw std::runtime_error(msg.str());
    }

    processNode(scene->mRootNode, scene);

    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* material = scene->mMaterials[i];

        // Diffuse color
        aiColor3D diffuseColor(0.f, 0.f, 0.f);
        material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
        this->diffuse.emplace_back(diffuseColor.r, diffuseColor.g, diffuseColor.b);

        // Ambient color
        aiColor3D ambientColor(0.f, 0.f, 0.f);
        material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor);
        this->ambient.emplace_back(ambientColor.r, ambientColor.g, ambientColor.b);

        // Specular color
        aiColor3D specularColor(0.f, 0.f, 0.f);
        material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor);
        this->specular.emplace_back(specularColor.r, specularColor.g, specularColor.b);
    }
}

ppgso::Mesh_Assimp::~Mesh_Assimp() {
    for(auto& buffer : buffers) {
        glDeleteBuffers(1, &buffer.ibo);
        glDeleteBuffers(1, &buffer.nbo);
        glDeleteBuffers(1, &buffer.tbo);
        glDeleteBuffers(1, &buffer.vbo);
        glDeleteVertexArrays(1, &buffer.vao);
    }
}

void ppgso::Mesh_Assimp::processNode(aiNode *node, const aiScene *pScene) {
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh *mesh = pScene->mMeshes[node->mMeshes[i]];
        processMesh(mesh);
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], pScene);
    }
}

void ppgso::Mesh_Assimp::processMesh(aiMesh *mesh) {
    gl_buffer buffer;

    // Process vertices
    if (mesh->HasPositions()) {
        // Generate a vertex array object
        glGenVertexArrays(1, &buffer.vao);
        glBindVertexArray(buffer.vao);

        // Extract vertex positions from aiMesh and upload to GPU
        glGenBuffers(1, &buffer.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * sizeof(aiVector3D), mesh->mVertices, GL_STATIC_DRAW);
        // Enable and set up vertex attribute pointer for positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    // Process texture coordinates
    if (mesh->HasTextureCoords(0)) {
        std::vector<aiVector2D> textureCoords;
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            aiVector3D texCoord = mesh->mTextureCoords[0][i]; // Assuming single texture channel (index 0)
            textureCoords.push_back(aiVector2D(texCoord.x, texCoord.y));
        }

        glGenBuffers(1, &buffer.tbo);
        glBindBuffer(GL_ARRAY_BUFFER, buffer.tbo);
        glBufferData(GL_ARRAY_BUFFER, textureCoords.size() * sizeof(aiVector2D), textureCoords.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }


    // Process normals
    if (mesh->HasNormals()) {
        std::vector<aiVector3D> normals;
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            aiVector3D normal = mesh->mNormals[i];
            normals.push_back(normal);
        }

        glGenBuffers(1, &buffer.nbo);
        glBindBuffer(GL_ARRAY_BUFFER, buffer.nbo);
        glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(aiVector3D), normals.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    }


    // Process indices
    if (mesh->HasFaces()) {
        std::vector<unsigned int> indices;
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(face.mIndices[j]);
            }
        }

        // Upload indices to GPU
        glGenBuffers(1, &buffer.ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        buffer.size = static_cast<GLsizei>(indices.size());
    }

    buffers.push_back(buffer);
}

void ppgso::Mesh_Assimp::render() {
    for (auto &buffer : buffers) {
        // Draw object
        glBindVertexArray(buffer.vao);
        glDrawElements(GL_TRIANGLES, buffer.size, GL_UNSIGNED_INT, nullptr);
    }
}
