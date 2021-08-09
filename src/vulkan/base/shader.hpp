#pragma once

#include <vulkan/vulkan.h>

namespace dp {

enum class ShaderStage : uint32_t {
    RayGeneration = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    RayMiss = VK_SHADER_STAGE_MISS_BIT_KHR
};

class ShaderModule {
private:
    VkShaderModule shaderModule;
    dp::ShaderStage shaderStage;

public:
    ShaderModule(VkShaderModule module, dp::ShaderStage shaderStage) : shaderModule(module), shaderStage(shaderStage) {}

    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo();

    dp::ShaderStage getShaderStage() {
        return shaderStage;
    }
};

} // namespace dp
