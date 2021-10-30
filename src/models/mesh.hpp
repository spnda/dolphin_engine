#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace fs = std::filesystem;

namespace dp {
    // An index type for the mesh.
    using Index = uint32_t;

    struct Vertex {
        // The relative vertex position.
        // Has to be at the start because of the AS.
        glm::fvec3 pos;
        glm::fvec3 normals;
        glm::fvec2 uv;
        float padding = 1.0f;
    };

    /**
     * Struct referencing buffer addresses for various mesh related buffers.
     * Used for the shader to obtain vertex data per mesh.
     */
    struct ObjectDescription {
        VkDeviceAddress vertexBufferAddress = 0;
        VkDeviceAddress indexBufferAddress = 0;
        Index materialIndex = 0;
    };

    struct Material {
        glm::vec3 diffuse = glm::vec3(1.0f);
        glm::vec3 specular = glm::vec3(1.0f);
        glm::vec3 emissive = glm::vec3(1.0f);
        Index textureIndex = 0; // 0 is just the default white image.
    };

    /** Represents a single texture file that can be uploaded to a dp::Texture later. */
    struct TextureFile {
        fs::path filePath;
        uint32_t width = 0, height = 0;
        uint32_t mipLevels = 1;
        std::vector<uint8_t> pixels = {};
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    struct Mesh {
        std::string name = {};
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices = {};
        VkTransformMatrixKHR transform;
        VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
        // Typically, just the size of a single Vertex.
        static const uint32_t vertexStride = sizeof(Vertex);
        // The material index for the material buffer.
        // We use this together with instanceCustomIndex to provide
        // material data to ray shaders.
        Index materialIndex = 0;
    };
}
