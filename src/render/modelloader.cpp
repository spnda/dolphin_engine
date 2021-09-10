#include "modelloader.hpp"

#include <chrono>
#include <iostream>

dp::ModelLoader::ModelLoader() : importer() {

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
        newMesh.vertices.push_back(vertex);
    }
    
    for (int i = 0; i < mesh->mNumFaces; i++) {
        for (int j = 0; j < meshFaces[i].mNumIndices; j++) {
            newMesh.indices.push_back(meshFaces[i].mIndices[j]);
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

bool dp::ModelLoader::loadFile(const std::string fileName) {
    printf("Importing file %s!\n", fileName.c_str());
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    const aiScene* scene = importer.ReadFile(fileName, importFlags);

    if (!scene || scene->mRootNode == nullptr) {
        std::cout << importer.GetErrorString() << std::endl;
        return false;
    }

    loadNode(scene->mRootNode, scene);

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    printf("Finished importing file. Took %zu[ms].\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());

    return true;
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
