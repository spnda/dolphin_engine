#include <vulkan/vulkan.h>

#include "rt_pipeline.hpp"
#include "../resource/buffer.hpp"
#include "../resource/uniform_data.hpp"

dp::RayTracingPipelineBuilder dp::RayTracingPipelineBuilder::create(Context& context, std::string pipelineName) {
    dp::RayTracingPipelineBuilder builder(context);
    builder.pipelineName = pipelineName;
    return builder;
}

dp::RayTracingPipelineBuilder dp::RayTracingPipelineBuilder::createDefaultDescriptorSets(const dp::Image& storageImage, const dp::Buffer& uboBuffer, const dp::AccelerationStructure& topLevelAS) {
    if (descriptorLayoutBindings.size() == 0) this->useDefaultDescriptorLayout();

    static std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
    vkCreateDescriptorPool(ctx.device.device, &descriptorPoolCreateInfo, nullptr, &descriptorPool);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    vkAllocateDescriptorSets(ctx.device.device, &descriptorSetAllocateInfo, &descriptorSet);

    // Acceleration structure descriptor
	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = {};
	descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.handle;

    VkWriteDescriptorSet accelerationStructureWrite = {};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
	accelerationStructureWrite.dstSet = descriptorSet;
	accelerationStructureWrite.dstBinding = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    // Storage image descriptor
	VkDescriptorImageInfo storageImageDescriptor{};
	storageImageDescriptor.imageView = storageImage.imageView;
	storageImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet resultImageWrite = {};
    resultImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	resultImageWrite.dstSet = descriptorSet;
	resultImageWrite.dstBinding = 1;
	resultImageWrite.descriptorCount = 1;
	resultImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resultImageWrite.pImageInfo = &storageImageDescriptor;

    // UBO image descriptor
    VkDescriptorBufferInfo uboBufferInfo = {};
    uboBufferInfo.buffer = uboBuffer.handle;
    uboBufferInfo.range = sizeof(UniformData);

    VkWriteDescriptorSet uboBufferWrite = {};
    uboBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uboBufferWrite.dstSet = descriptorSet;
    uboBufferWrite.dstBinding = 2;
    uboBufferWrite.descriptorCount = 1;
    uboBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBufferWrite.pBufferInfo = &uboBufferInfo;
	//VkWriteDescriptorSet uniformBufferWrite = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &ubo.descriptor);

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		accelerationStructureWrite,
		resultImageWrite,
		uboBufferWrite
	};
	vkUpdateDescriptorSets(ctx.device.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);

    return *this;
}

dp::RayTracingPipelineBuilder dp::RayTracingPipelineBuilder::useDefaultDescriptorLayout() {
    VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding = {};
    accelerationStructureLayoutBinding.binding = 0;
    accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    accelerationStructureLayoutBinding.descriptorCount = 1;
    accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding resultImageLayoutBinding = {};
    resultImageLayoutBinding.binding = 1;
    resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resultImageLayoutBinding.descriptorCount = 1;
    resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding uniformBufferBinding = {};
    uniformBufferBinding.binding = 2;
    uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBufferBinding.descriptorCount = 1;
    uniformBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    descriptorLayoutBindings.push_back(accelerationStructureLayoutBinding);
    descriptorLayoutBindings.push_back(resultImageLayoutBinding);
    descriptorLayoutBindings.push_back(uniformBufferBinding);

    VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo = {};
    descriptorLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorLayoutBindings.size());
    descriptorLayoutCreateInfo.pBindings = descriptorLayoutBindings.data();
    vkCreateDescriptorSetLayout(ctx.device.device, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayout);

    return *this;
}

dp::RayTracingPipelineBuilder dp::RayTracingPipelineBuilder::addShader(dp::ShaderModule module) {
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

dp::RayTracingPipeline dp::RayTracingPipelineBuilder::build() {
    dp::RayTracingPipeline pipeline = {};

    pipeline.descriptorSet = descriptorSet;
    // pipeline.descriptorSets.push_back(descriptorSet);
    // std::copy(descriptorSets.begin(), descriptorSets.end(), std::inserter(pipeline.descriptorSets, pipeline.descriptorSets.end()));

    pipeline.descriptorLayout = descriptorSetLayout;

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &pipeline.descriptorLayout;
    vkCreatePipelineLayout(ctx.device.device, &pipelineLayoutCreateInfo, nullptr, &pipeline.pipelineLayout);

    // Create RT pipeline
    VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
    pipelineCreateInfo.pGroups = shaderGroups.data();
    pipelineCreateInfo.layout = pipeline.pipelineLayout;

    // Stupid extension shit.
    reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(ctx.device.device, "vkCreateRayTracingPipelinesKHR"))
        (ctx.device.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline.pipeline);
    
    ctx.setDebugUtilsName(pipeline.pipeline, pipelineName);
    return pipeline;
}
