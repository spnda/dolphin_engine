#pragma once

#include <glm/glm.hpp>

namespace dp {

    using Index = uint32_t;

    struct Vertex {
        glm::vec3 pos;
    };

    struct Material {
        glm::vec3 diffuse = glm::vec3(1.0f);
        glm::vec3 emissive = glm::vec3(1.0f);
    };

    struct Mesh {
        std::string name = {};
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices = {};
        VkTransformMatrixKHR transform;

        Material material;
    };
}
