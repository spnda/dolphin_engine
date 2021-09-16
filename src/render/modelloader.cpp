#include "modelloader.hpp"

#include <chrono>
#include <iostream>

dp::ModelLoader::ModelLoader(const dp::Context& context)
        : ctx(context), importer(), materialBuffer(ctx, "materialBuffer") {

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
        vertex.materialIndex = mesh->mMaterialIndex;
        newMesh.vertices.push_back(vertex);
    }

    for (int i = 0; i < mesh->mNumFaces; i++) {
        for (int j = 0; j < meshFaces[i].mNumIndices; j++) {
            aiFace face = meshFaces[i];
            newMesh.indices.push_back(face.mIndices[j]);
        }
    }

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

void dp::ModelLoader::getMatColor3(aiMaterial* material, const char* key, unsigned int type, unsigned int idx, glm::vec4* vec) const {
    aiColor3D diffuse(0.0f, 0.0f, 0.0f);
    material->Get(key, type, idx, &diffuse, nullptr);
    vec->b = diffuse.b;
    vec->r = diffuse.r;
    vec->g = diffuse.g;
}

bool dp::ModelLoader::loadFile(const std::string fileName) {
    printf("Importing file %s!\n", fileName.c_str());
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

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
            getMatColor3(mat, AI_MATKEY_COLOR_EMISSIVE, &material.emissive);
            materials.push_back(material);
        }
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    printf("Finished importing file. Took %zu[ms].\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());

    return true;
}

void dp::ModelLoader::createMaterialBuffer() {
    materialBuffer.create(
        materials.size() * sizeof(Material),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
    );
    materialBuffer.memoryCopy(materials.data(), materials.size() * sizeof(Material));
}

dp::AccelerationStructure dp::ModelLoader::buildAccelerationStructure(const dp::Context& context) {
    auto builder = dp::AccelerationStructureBuilder::create(context);
    for (auto mesh : meshes) {
        uint32_t meshIndex = builder.addMesh(dp::AccelerationStructureMesh {
            .vertices = mesh.vertices,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .indices = mesh.indices,
            .indexType = VK_INDEX_TYPE_UINT32,
            .stride = sizeof(Vertex),
            .transformMatrix = mesh.transform,
        });
        builder.addInstance(dp::AccelerationStructureInstance {
            .transformMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f
            },
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .blasIndex = meshIndex,
        });
    }
    return builder.build();
}
