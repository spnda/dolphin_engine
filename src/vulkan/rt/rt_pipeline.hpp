#pragma once

#include <string>
#include <vector>

namespace dp {
    // fwd
    class Context;
    class ShaderModule;

    class RayTracingPipeline {
        PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;

    public:
        VkPipeline pipeline = nullptr;
        VkPipelineLayout pipelineLayout = nullptr;

        VkDescriptorPool descriptorPool = nullptr;
        VkDescriptorSetLayout descriptorLayout = nullptr;
        VkDescriptorSet descriptorSet = nullptr;

        explicit operator VkPipeline() const;

        void destroy(const dp::Context& ctx) const;
    };

    class RayTracingPipelineBuilder {
        Context& ctx;
        std::string pipelineName;
        
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

        VkDescriptorPool descriptorPool = nullptr;
        VkDescriptorSet descriptorSet = nullptr;
        VkDescriptorSetLayout descriptorSetLayout = nullptr;
        std::vector<VkDescriptorSetLayoutBinding> descriptorLayoutBindings;
        std::vector<VkWriteDescriptorSet> descriptorWrites;
        VkPushConstantRange pushConstants;

        explicit RayTracingPipelineBuilder(Context& context) : ctx(context) {}

    public:
        static RayTracingPipelineBuilder create(Context& context, std::string pipelineName);

        RayTracingPipelineBuilder& addShader(const ShaderModule& module);
        RayTracingPipelineBuilder& addImageDescriptor(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, dp::ShaderStage stageFlags, uint32_t count = 1);
        RayTracingPipelineBuilder& addBufferDescriptor(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, dp::ShaderStage stageFlags, uint32_t count = 1);
        RayTracingPipelineBuilder& addAccelerationStructureDescriptor(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo, VkDescriptorType type, dp::ShaderStage stageFlags);
        RayTracingPipelineBuilder& addPushConstants(uint32_t pushConstantSize, dp::ShaderStage shaderStage);

        // Builds the descriptor set layout and pipeline layout, then creates
        // a VkRayTracingPipeline based on that.
        RayTracingPipeline build();
    };
} // namespace dp
