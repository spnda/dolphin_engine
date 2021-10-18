#define STB_IMAGE_IMPLEMENTATION
#include "modelloader.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

#include <stb_image.h>
#include <vk_mem_alloc.h>
#include <fmt/core.h>

#include "../vulkan/rt/acceleration_structure_builder.hpp"
#include "../vulkan/rt/acceleration_structure.hpp"

namespace fs = std::filesystem;

dp::ModelLoader::ModelLoader(const dp::Context& context)
        : ctx(context), importer(),
        materialBuffer(ctx, "materialBuffer"), descriptionsBuffer(ctx, "descriptionsBuffer") {

}

void dp::ModelLoader::loadMesh(const aiMesh *mesh, const aiMatrix4x4 transform) {
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
        Vertex vertex {
            .pos = glm::vec4(meshVertices[i].x, meshVertices[i].y, meshVertices[i].z, 1.0),
            .normals = (mesh->HasNormals())
                       ? glm::vec4(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z, 1.0)
                       : glm::vec4(1.0),
            .uv = mesh->mTextureCoords[0] == nullptr
                ? glm::vec2(0.0)
                : glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y),
        };
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
            loadMesh(scene->mMeshes[node->mMeshes[i]], node->mTransformation);
        }
    }
    if (node->mChildren != nullptr) {
        for (int i = 0; i < node->mNumChildren; i++) {
            loadNode(node->mChildren[i], scene);
        }
    }
}

bool dp::ModelLoader::loadTexture(const std::string& path) {
    fmt::print("Importing texture {}\n", path);

    // Read the texture file.
    uint32_t width, height, channels;
    void* pixels;
    {
        int tWidth = 0, tHeight = 0, tChannels = 0;
        stbi_uc* stbPixels = stbi_load(path.c_str(), &tWidth, &tHeight, &tChannels, STBI_rgb_alpha);
        if (!stbPixels) {
            std::cerr << "Failed to load texture file " << path << std::endl;
            return false;
        }
        width = tWidth; height = tHeight; channels = tChannels;
        pixels = stbPixels;
    }

    uint64_t imageSize = width * height * 4;
    dp::Texture texture(ctx, { width, height }, fs::path(path).filename().string());
    texture.create();

    // Stage the pixels in a buffer
    dp::StagingBuffer stagingBuffer(ctx, "textureStagingBuffer");
    stagingBuffer.create(imageSize);
    stagingBuffer.memoryCopy(pixels, imageSize);
    stbi_image_free(pixels);

    // Change layout and copy the buffer to the image
    ctx.oneTimeSubmit(ctx.graphicsQueue, ctx.commandPool, [&](VkCommandBuffer cmdBuffer) {
        texture.changeLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent = texture.getImageSize3d(),
        };
        stagingBuffer.copyToImage(cmdBuffer, texture, texture.getImageLayout(), &copy);
        texture.changeLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    });
    stagingBuffer.destroy();

    textures.push_back(texture);
    return true;
}

void dp::ModelLoader::loadEmbeddedTexture(const aiTexture* embeddedTexture) {
    // TODO: Support embedded textures.
    // loadTexture(embeddedTexture->mFilename.C_Str());
}

inline void dp::ModelLoader::getMatColor3(aiMaterial* material, const char* key, unsigned int type, unsigned int idx, glm::vec4* vec) {
    aiColor4D vec4;
    const aiReturn ret = aiGetMaterialColor(material, key, type, idx, &vec4);
    if (ret != aiReturn_SUCCESS) {
        std::cerr << "Encountered assimp error" << std::endl;
        vec4 = aiColor4D(0.0f);
    }
    vec->b = vec4.b;
    vec->r = vec4.r;
    vec->g = vec4.g;
}

bool dp::ModelLoader::loadFile(const std::string& fileName) {
    fmt::print("Importing file {}!\n", fileName);
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    const aiScene* scene = importer.ReadFile(fileName, importFlags);

    if (!scene || scene->mRootNode == nullptr) {
        fmt::print(stderr, "{}\n", importer.GetErrorString());
        return false;
    }

    // Load Meshes
    loadNode(scene->mRootNode, scene);

    // Load default, empty, white texture.
    dp::Texture defaultTexture(ctx, { 1, 1 });
    defaultTexture.create();
    dp::StagingBuffer stagingBuffer(ctx, "defaultTextureStagingBuffer");
    stagingBuffer.create(4);
    std::vector<uint8_t> emptyImage = { 0xFF, 0xFF, 0xFF, 0xFF };
    stagingBuffer.memoryCopy(emptyImage.data(), 4);

    ctx.oneTimeSubmit(ctx.graphicsQueue, ctx.commandPool, [&](VkCommandBuffer cmdBuffer) {
        defaultTexture.changeLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent = { 1, 1, 1 },
        };
        stagingBuffer.copyToImage(cmdBuffer, defaultTexture, defaultTexture.getImageLayout(), &copy);
        defaultTexture.changeLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    });
    stagingBuffer.destroy();

    textures.push_back(defaultTexture);

    // Load Materials
    if (scene->HasMaterials()) {
        for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
            dp::Material material;
            aiMaterial* mat = scene->mMaterials[i];
            getMatColor3(mat, AI_MATKEY_COLOR_DIFFUSE, &material.diffuse);
            getMatColor3(mat, AI_MATKEY_COLOR_SPECULAR, &material.specular);
            getMatColor3(mat, AI_MATKEY_COLOR_EMISSIVE, &material.emissive);

            // Load each diffuse texture. For now, we only support a single texture here.
            for (uint32_t j = 0; j < 1 /* mat->GetTextureCount(aiTextureType_DIFFUSE) */; j++) {
                aiString texturePath;
                mat->GetTexture(aiTextureType_DIFFUSE, j, &texturePath);

                const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(texturePath.C_Str());
                if (embeddedTexture != nullptr) {
                    loadEmbeddedTexture(embeddedTexture);
                } else {
                    // Take the path of the model as a relative path.
                    fs::path parentFolder = fs::path(fileName).parent_path();
                    bool loaded = loadTexture(parentFolder.string() + "\\" + texturePath.C_Str());
                    if (loaded) {
                        material.textureIndex = static_cast<int32_t>(textures.size() - 1);
                    }
                }
            }

            materials.push_back(material);
        }
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    fmt::print("Finished importing file. Took {}ms.\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());

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

void dp::ModelLoader::createDescriptionsBuffer(const dp::TopLevelAccelerationStructure& tlas) {
    // TODO: Make this less cursed.
    descriptions.resize(meshes.size());
    for (const auto& mesh : meshes) {
        uint64_t index = &mesh - &meshes[0];
        auto blas = tlas.blases[index];
        ObjectDescription desc = {
            .vertexBufferAddress = blas.vertexBuffer.getDeviceOrHostAddress().deviceAddress,
            .indexBufferAddress = blas.indexBuffer.getDeviceOrHostAddress().deviceAddress,
            .materialIndex = mesh.materialIndex,
        };
        descriptions[index] = desc;
    }

    descriptionsBuffer.create(
        descriptions.size() * sizeof(ObjectDescription),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    descriptionsBuffer.memoryCopy(descriptions.data(), descriptions.size() * sizeof(ObjectDescription));
}

std::vector<VkDescriptorImageInfo> dp::ModelLoader::getTextureDescriptorInfos() {
    // Create the image infos for each texture
    std::vector<VkDescriptorImageInfo> descriptors(textures.size());
    for (size_t i = 0; i < descriptors.size(); i++) {
        descriptors[i].sampler = textures[i].getSampler();
        descriptors[i].imageLayout = textures[i].getImageLayout();
        descriptors[i].imageView = VkImageView(textures[i]);
    }
    return descriptors;
}

dp::TopLevelAccelerationStructure dp::ModelLoader::buildAccelerationStructure(const dp::Context& context) {
    auto builder = dp::AccelerationStructureBuilder::create(context, context.commandPool);
    for (const auto& mesh : meshes) {
        uint32_t meshIndex = builder.addMesh(mesh);

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
