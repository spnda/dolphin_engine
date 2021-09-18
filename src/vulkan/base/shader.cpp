#include "shader.hpp"

dp::ShaderModule::ShaderModule(VkShaderModule module, dp::ShaderStage shaderStage)
        : shaderModule(module), shaderStage(shaderStage) {
    
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
