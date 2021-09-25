#pragma once

#include <string>
#include <vulkan/vulkan.h>

#include "../resource/buffer.hpp"

namespace dp {
    // fwd
    class Context;
    
    enum AccelerationStructureType : uint32_t {
        TopLevel = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        BottomLevel = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        Generic = VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR
    };

    struct AccelerationStructureInstance {
        VkTransformMatrixKHR transformMatrix;
        VkGeometryInstanceFlagBitsKHR flags;
        uint32_t blasIndex; // TODO: Have a better system of referencing the mesh.
    };

    class AccelerationStructure {
        const dp::Context& ctx;

    public:
        std::string name;
        dp::AccelerationStructureType type = AccelerationStructureType::Generic;
        VkAccelerationStructureKHR handle = nullptr;
        VkDeviceAddress address = 0;
        dp::Buffer resultBuffer;
        
        /** If this->type is TopLevel, this should be a list of all relevant BLASes. */
        std::vector<AccelerationStructure> children;

        AccelerationStructure(const dp::Context& context, const AccelerationStructureType type = AccelerationStructureType::Generic, const std::string asName = "blas");
        AccelerationStructure(const AccelerationStructure& structure);

        AccelerationStructure& operator=(const dp::AccelerationStructure& structure);

        void setName();
    };
}
