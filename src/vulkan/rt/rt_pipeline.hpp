#pragma once

#include "../context.hpp"
#include "../base/shader.hpp"

namespace dp {

class TopLevelAccelerationStructure;
class Buffer;

struct RayTracingPipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorSet descriptorSet;
};

class RayTracingPipelineBuilder {
    Context& ctx;
    std::string pipelineName;
    
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSetLayoutBinding> descriptorLayoutBindings;

    RayTracingPipelineBuilder(Context& context) : ctx(context) {}

public:
    static RayTracingPipelineBuilder create(Context& context, std::string pipelineName);

    // Creates the default descriptor sets.
    // Calls useDefaultDescriptorLayout if not already done.
    RayTracingPipelineBuilder createDefaultDescriptorSets(const dp::Image& storageImage, const dp::Buffer& uboBuffer, const dp::TopLevelAccelerationStructure& topLevelAS);

    // Also builds the descriptor set layout.
    RayTracingPipelineBuilder useDefaultDescriptorLayout();

    RayTracingPipelineBuilder addShader(ShaderModule module);

    // Builds the descriptor set layout and pipeline layout, then creates
    // a VkRayTracingPipeline based on that.
    RayTracingPipeline build();
};

} // namespace dp
