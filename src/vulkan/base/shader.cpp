#include "shader.hpp"

#include "../context.hpp"

dp::ShaderModule::ShaderModule() {}

dp::ShaderModule::ShaderModule(VkShaderModule module, dp::ShaderStage shaderStage)
        : shaderModule(module), shaderStage(shaderStage) {
    
}

dp::ShaderModule dp::ShaderModule::createShader(const dp::Context& ctx, const std::string shaderCode, const dp::ShaderStage shaderStage) {
    dp::ShaderModule shaderModule;
    shaderModule.shaderStage = shaderStage;
    VkShaderModuleCreateInfo moduleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = shaderCode.size(),
        .pCode = (uint32_t*)shaderCode.data(),
    };
    vkCreateShaderModule(ctx.device, &moduleCreateInfo, nullptr, &shaderModule.shaderModule);
    return shaderModule;
}

VkPipelineShaderStageCreateInfo dp::ShaderModule::getShaderStageCreateInfo() {
    VkPipelineShaderStageCreateInfo stageCreateInfo = {};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.stage = static_cast<VkShaderStageFlagBits>(this->shaderStage);
    stageCreateInfo.module = this->shaderModule;
    stageCreateInfo.pName = "main"; // We will just use this by default.
    return stageCreateInfo;
}

dp::ShaderStage dp::ShaderModule::getShaderStage() {
    return shaderStage;
}
