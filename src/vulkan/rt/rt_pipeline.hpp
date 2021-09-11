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
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    RayTracingPipelineBuilder(Context& context) : ctx(context) {}

public:
    static RayTracingPipelineBuilder create(Context& context, std::string pipelineName);

    RayTracingPipelineBuilder& addShader(ShaderModule module);
    RayTracingPipelineBuilder& addImageDescriptor(const uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
    RayTracingPipelineBuilder& addBufferDescriptor(const uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
    RayTracingPipelineBuilder& addAccelerationStructureDescriptor(const uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);

    // Builds the descriptor set layout and pipeline layout, then creates
    // a VkRayTracingPipeline based on that.
    RayTracingPipeline build();
};

} // namespace dp
