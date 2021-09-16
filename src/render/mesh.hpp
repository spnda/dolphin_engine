#pragma once

#include <glm/glm.hpp>

namespace dp {

    using Index = uint32_t;

    struct Vertex {
        // The relative vertex position.
        // Has to be at the start because of the AS.
        glm::vec3 pos;
        // The material index for the material buffer.
        uint32_t materialIndex;
    };

    struct Material {
        glm::vec4 diffuse = glm::vec4(1.0f);
        glm::vec4 emissive = glm::vec4(1.0f);
    };

    struct Mesh {
        std::string name = {};
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices = {};
        VkTransformMatrixKHR transform;
    };
}
