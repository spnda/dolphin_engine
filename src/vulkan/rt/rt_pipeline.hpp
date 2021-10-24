#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace dp {
    // fwd
    class Context;
    class ShaderModule;

    enum class RtShaderGroup : uint32_t {
        General = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        TriangleHit = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        Procedural = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
    };

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

        RayTracingPipelineBuilder& addShaderGroup(RtShaderGroup group, std::initializer_list<dp::ShaderModule> shaders);
        RayTracingPipelineBuilder& addImageDescriptor(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, dp::ShaderStage stageFlags, uint32_t count = 1);
        RayTracingPipelineBuilder& addBufferDescriptor(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, dp::ShaderStage stageFlags, uint32_t count = 1);
        RayTracingPipelineBuilder& addAccelerationStructureDescriptor(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo, VkDescriptorType type, dp::ShaderStage stageFlags);
        RayTracingPipelineBuilder& addPushConstants(uint32_t pushConstantSize, dp::ShaderStage shaderStage);

        // Builds the descriptor set layout and pipeline layout, then creates
        // a VkRayTracingPipeline based on that.
        RayTracingPipeline build();
    };
} // namespace dp
