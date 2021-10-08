#include "shader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "../context.hpp"
#include "file_includer.hpp"

static std::map<dp::ShaderStage, shaderc_shader_kind> shader_kinds {
    { dp::ShaderStage::RayGeneration, shaderc_raygen_shader },
    { dp::ShaderStage::ClosestHit, shaderc_closesthit_shader },
    { dp::ShaderStage::RayMiss, shaderc_miss_shader }
};

dp::ShaderModule::ShaderModule(const dp::Context& context, std::string name, const dp::ShaderStage shaderStage)
        : ctx(context), name(std::move(name)), shaderStage(shaderStage) {
    
}

std::string dp::ShaderModule::readFile(const std::string& filename) {
    std::ifstream is(filename, std::ios::binary);

    if (is.is_open()) {
        std::string shaderCode;
        std::stringstream stringBuffer;
        stringBuffer << is.rdbuf();
        shaderCode = stringBuffer.str();

        return shaderCode;
    } else {
        throw std::runtime_error(std::string("Failed to open shader file: ") + filename);
    }
}

std::vector<uint32_t> dp::ShaderModule::compileShader(const std::string& shaderName, const std::string& shader_source) const {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    // TODO: Macro definitions

    std::unique_ptr<FileIncluder> includer(new FileIncluder());
    options.SetIncluder(std::move(includer));
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetSpirv(shaderc_spirv_version_1_5);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

    shaderc_shader_kind kind = shader_kinds[shaderStage];

    auto checkResult = [=]<typename T>(shaderc::CompilationResult<T>& result) mutable {
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::cerr << result.GetErrorMessage() << std::endl;
            return std::vector<T>();
        }
        return std::vector<T>(result.cbegin(),  result.cend());
    };

    // Preprocess the files.
    auto result = compiler.PreprocessGlsl(shader_source, kind, shaderName.c_str(), options);
    const std::vector<char> preProcessedSourceChars = checkResult(result);
    const std::string preProcessedSource = {preProcessedSourceChars.begin(), preProcessedSourceChars.end()};

    // Compile to SPIR-V
    auto compileResult = compiler.CompileGlslToSpv(preProcessedSource, kind, shaderName.c_str(), options);
    return checkResult(compileResult);
}

void dp::ShaderModule::createShaderModule() {
    VkShaderModuleCreateInfo moduleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = shaderBinary.size() * 4,
        .pCode = shaderBinary.data(),
    };

    vkCreateShaderModule(ctx.device, &moduleCreateInfo, nullptr, &shaderModule);

    ctx.setDebugUtilsName(shaderModule, name);
}

void dp::ShaderModule::createShader(const std::string& filename) {
    auto fileContents = readFile(filename);
    shaderBinary = compileShader(filename, fileContents);
    createShaderModule();
}

VkPipelineShaderStageCreateInfo dp::ShaderModule::getShaderStageCreateInfo() const {
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = static_cast<VkShaderStageFlagBits>(this->shaderStage),
        .module = this->shaderModule,
        .pName = "main",
    };
}

dp::ShaderStage dp::ShaderModule::getShaderStage() const {
    return shaderStage;
}

VkShaderModule dp::ShaderModule::getHandle() const {
    return shaderModule;
}
