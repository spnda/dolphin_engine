#pragma once

#include "acceleration_structure.hpp"
#include "bottom_level_geometry.hpp"

namespace dp {
    class BottomLevelAccelerationStructure final : public AccelerationStructure {
    public:
        BottomLevelAccelerationStructure(const dp::Context& context, const dp::BottomLevelGeometry& geometry);

        void generate(VkCommandBuffer commandBuffer, dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkDeviceSize resultOffset);
    
    private:
        BottomLevelGeometry geometries;
    };
}
