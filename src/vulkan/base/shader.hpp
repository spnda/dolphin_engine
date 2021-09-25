#pragma once

#include <string>
#include <vulkan/vulkan.h>

namespace dp {
    class Context;

    enum ShaderStage : uint32_t {
        RayGeneration = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        AnyHit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        RayMiss = VK_SHADER_STAGE_MISS_BIT_KHR,
        Intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
        Callable = VK_SHADER_STAGE_CALLABLE_BIT_KHR
    };

    class ShaderModule {
        VkShaderModule shaderModule;
        dp::ShaderStage shaderStage;

        ShaderModule();
    public:
        ShaderModule(VkShaderModule module, dp::ShaderStage shaderStage);

        static dp::ShaderModule createShader(const dp::Context& ctx, const std::string shaderCode, const dp::ShaderStage shaderStage);

        VkPipelineShaderStageCreateInfo getShaderStageCreateInfo();

        dp::ShaderStage getShaderStage();
    };
} // namespace dp
