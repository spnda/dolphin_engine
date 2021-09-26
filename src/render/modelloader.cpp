#include "modelloader.hpp"

#include <vk_mem_alloc.h>

#include <chrono>
#include <iostream>

#include "../vulkan/rt/acceleration_structure.hpp"
#include "../vulkan/context.hpp"

dp::ModelLoader::ModelLoader(const dp::Context& context)
        : ctx(context), importer() {
}

void dp::ModelLoader::loadMesh(const aiMesh* mesh, const aiMatrix4x4 transform, const aiScene* scene) {
    if (!mesh->HasFaces()) return;
    auto meshVertices = mesh->mVertices;
    auto meshFaces = mesh->mFaces;

    dp::Mesh newMesh;
    newMesh.name = mesh->mName.data;
    newMesh.transform = {
        transform.a1, transform.a2, transform.a3, transform.a4,
        transform.b1, transform.b2, transform.b3, transform.b4,
        transform.c1, transform.c2, transform.c3, transform.c4
    };

    for (int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.pos = glm::vec3(meshVertices[i].x, meshVertices[i].y, meshVertices[i].z);
        if (mesh->HasNormals()) {
            vertex.normals = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }
        newMesh.vertices.push_back(vertex);
    }

    for (int i = 0; i < mesh->mNumFaces; i++) {
        for (int j = 0; j < meshFaces[i].mNumIndices; j++) {
            aiFace face = meshFaces[i];
            newMesh.indices.push_back(face.mIndices[j]);
        }
    }

    newMesh.materialIndex = mesh->mMaterialIndex;

    meshes.push_back(newMesh);
}

void dp::ModelLoader::loadNode(const aiNode* node, const aiScene* scene) {
    if (node->mMeshes != nullptr) {
        for (int i = 0; i < node->mNumMeshes; i++) {
            loadMesh(scene->mMeshes[node->mMeshes[i]], node->mTransformation, scene);
        }
    }
    if (node->mChildren != nullptr) {
        for (int i = 0; i < node->mNumChildren; i++) {
            loadNode(node->mChildren[i], scene);
        }
    }
}

inline void dp::ModelLoader::getMatColor3(aiMaterial* material, const char* key, unsigned int type, unsigned int idx, glm::vec4* vec) const {
    aiColor4D vec4;
    const aiReturn ret = aiGetMaterialColor(material, key, type, idx, &vec4);
    vec->b = vec4.b;
    vec->r = vec4.r;
    vec->g = vec4.g;
}

bool dp::ModelLoader::loadFile(const std::string fileName) {
    const aiScene* scene = importer.ReadFile(fileName, importFlags);

    if (!scene || scene->mRootNode == nullptr) {
        std::cout << importer.GetErrorString() << std::endl;
        return false;
    }

    // Load Meshes
    loadNode(scene->mRootNode, scene);

    // Load Materials
    if (scene->HasMaterials()) {
        for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
            dp::Material material;
            aiMaterial* mat = scene->mMaterials[i];
            getMatColor3(mat, AI_MATKEY_COLOR_DIFFUSE, &material.diffuse);
            getMatColor3(mat, AI_MATKEY_COLOR_SPECULAR, &material.specular);
            getMatColor3(mat, AI_MATKEY_COLOR_EMISSIVE, &material.emissive);
            materials.push_back(material);
        }
    }

    return true;
}
