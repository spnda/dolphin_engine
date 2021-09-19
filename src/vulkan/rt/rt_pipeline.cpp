#include <vulkan/vulkan.h>

#include "../context.hpp"
#include "rt_pipeline.hpp"
#include "../resource/buffer.hpp"
#include "../resource/uniform_data.hpp"

dp::RayTracingPipeline::operator VkPipeline() const {
    return pipeline;
}

void dp::RayTracingPipeline::destroy(const dp::Context& ctx) {
    vkDestroyPipeline(ctx.device, pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, descriptorLayout, nullptr);
}

dp::RayTracingPipelineBuilder dp::RayTracingPipelineBuilder::create(Context& context, std::string pipelineName) {
    dp::RayTracingPipelineBuilder builder(context);
    builder.pipelineName = pipelineName;
    return builder;
}

dp::RayTracingPipelineBuilder& dp::RayTracingPipelineBuilder::addShader(dp::ShaderModule module) {
    shaderStages.push_back(module.getShaderStageCreateInfo());

    // Create the shader group for this shader.
    // Ray generation and ray miss are both classified as a general shader.
    // As we only use closest hit, we'll simply switch between the two to 
    // use the appropriate data.
    VkRayTracingShaderGroupCreateInfoKHR shaderGroup = {};
    shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroup.type = module.getShaderStage() == dp::ShaderStage::ClosestHit
            ? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR
            : VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    switch (module.getShaderStage()) {
        case dp::ShaderStage::RayGeneration:
        case dp::ShaderStage::RayMiss:
            shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
            shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
            break;
        case dp::ShaderStage::ClosestHit:
            shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
            shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
            break;
    }
    shaderGroups.push_back(shaderGroup);

    return *this;
}

dp::RayTracingPipelineBuilder& dp::RayTracingPipelineBuilder::addImageDescriptor(const uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding newBinding = {
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = stageFlags,
        .pImmutableSamplers = nullptr,
    };

    descriptorLayoutBindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = imageInfo,
    };
    descriptorWrites.push_back(newWrite);

    return *this;
}

dp::RayTracingPipelineBuilder& dp::RayTracingPipelineBuilder::addBufferDescriptor(const uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding newBinding = {
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = stageFlags,
        .pImmutableSamplers = nullptr,
    };

    descriptorLayoutBindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = bufferInfo,
    };
    descriptorWrites.push_back(newWrite);

    return *this;
}

dp::RayTracingPipelineBuilder& dp::RayTracingPipelineBuilder::addAccelerationStructureDescriptor(const uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding newBinding = {
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = stageFlags,
        .pImmutableSamplers = nullptr,
    };

    descriptorLayoutBindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = asInfo,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
    };
    descriptorWrites.push_back(newWrite);

    return *this;
}

dp::RayTracingPipeline dp::RayTracingPipelineBuilder::build() {
    // Create the descriptor set layout.
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo = {};
    descriptorLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorLayoutBindings.size());
    descriptorLayoutCreateInfo.pBindings = descriptorLayoutBindings.data();
    vkCreateDescriptorSetLayout(ctx.device, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayout);

    // Create descriptor pool and allocate sets.
    static std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };

    ctx.createDescriptorPool(1, poolSizes, &descriptorPool);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    vkAllocateDescriptorSets(ctx.device, &descriptorSetAllocateInfo, &descriptorSet);

    // Copy the just allocated descriptor set to the write descriptors.
    // Then update the sets.
    for (auto& write : descriptorWrites) {
        write.dstSet = descriptorSet;
    }
    vkUpdateDescriptorSets(ctx.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);

    dp::RayTracingPipeline pipeline = {};
    pipeline.descriptorSet = descriptorSet;
    pipeline.descriptorLayout = descriptorSetLayout;

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &pipeline.descriptorLayout;
    vkCreatePipelineLayout(ctx.device, &pipelineLayoutCreateInfo, nullptr, &pipeline.pipelineLayout);

    // Create RT pipeline
    VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
    pipelineCreateInfo.pGroups = shaderGroups.data();
    pipelineCreateInfo.layout = pipeline.pipelineLayout;

    ctx.buildRayTracingPipeline(&pipeline.pipeline, { pipelineCreateInfo });

    ctx.setDebugUtilsName(pipeline.pipeline, pipelineName);
    return pipeline;
}
