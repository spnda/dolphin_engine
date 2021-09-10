#pragma once

#include "../../render/camera.hpp"
#include "../resource/storageimage.hpp"
#include "../context.hpp"
#include "../base/shader.hpp"
#include "./acceleration_structure_builder.hpp"

namespace dp {

class Buffer;

struct RayTracingPipeline {
private:
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;

public:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorSet descriptorSet;
};

class RayTracingPipelineBuilder {
private:
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
    RayTracingPipelineBuilder createDefaultDescriptorSets(const dp::StorageImage& storageImage, const dp::Camera& camera, const dp::AccelerationStructure& topLevelAS);

    // Also builds the descriptor set layout.
    RayTracingPipelineBuilder useDefaultDescriptorLayout();

    RayTracingPipelineBuilder addShader(ShaderModule module);

    // Builds the descriptor set layout and pipeline layout, then creates
    // a VkRayTracingPipeline based on that.
    RayTracingPipeline build();
};

} // namespace dp
