#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace fs = std::filesystem;

namespace dp {
    // An index type for the mesh.
    using Index = int32_t;

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
     * Used for the shader to obtain vertex and material data per primitive.
     */
    struct InstanceDescription {
        VkDeviceAddress vertexBufferAddress = 0;
        VkDeviceAddress indexBufferAddress = 0;
        VkDeviceAddress geometryBufferAddress = 0;
    };

    struct GeometryDescription {
        uint64_t meshBufferVertexOffset = 0;
        uint64_t meshBufferIndexOffset = 0;
        Index materialIndex = 0;
    };

    struct Material {
        glm::vec3 baseColor = glm::vec3(1.0f);
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        Index baseTextureIndex = -1; // -1 + 1 is 0, our default white image.
        Index normalTextureIndex = -1;
        Index occlusionTextureIndex = -1;
        Index emissiveTextureIndex = -1;
        Index pbrTextureIndex = -1;
    };

    /** Represents a single texture file that can be uploaded to a dp::Texture later. */
    struct TextureFile {
        fs::path filePath;
        uint32_t width = 0, height = 0;
        uint32_t mipLevels = 1;
        std::vector<uint8_t> pixels = {};
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    /**
     * Represents a small piece in geometry, with a transform matrix, vertices,
     * indices and a material.
     */
    struct Primitive {
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices = {};
        /** Accessed through gl_InstanceCustomIndexEXT and gl_GeometryIndexEXT. */
        Index materialIndex = 0;
        /* Can be set to VK_INDEX_TYPE_NONE_KHR if no indices are provided. */
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
        static const VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        static constexpr uint32_t vertexStride = sizeof(Vertex);
        /** These are offsets into the resulting mesh buffer for
         * specifically this primitive. */
        uint64_t meshBufferVertexOffset;
        uint64_t meshBufferIndexOffset;
    };

    struct Mesh {
        std::string name = {};
        std::vector<dp::Primitive> primitives = {};
        VkTransformMatrixKHR transform = {
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0
        };
    };
}
