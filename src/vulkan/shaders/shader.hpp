#pragma once

#include <map>

#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp>

namespace dp {
    enum class ShaderStage : uint64_t {
        RayGeneration = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        RayMiss = VK_SHADER_STAGE_MISS_BIT_KHR,
        AnyHit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        Intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
        Callable = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
    };

    inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
        return static_cast<ShaderStage>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
    }

    inline bool operator>(ShaderStage a, ShaderStage b) {
        return static_cast<uint64_t>(a) > static_cast<uint64_t>(b);
    }

    // fwd.
    class Context;

    struct ShaderCompileResult {
        std::vector<uint32_t> binary;
        std::vector<uint32_t> debugBinary;
    };

    class ShaderModule {
        const dp::Context& ctx;
        std::string name;

        VkShaderModule shaderModule = nullptr;
        dp::ShaderStage shaderStage;

        ShaderCompileResult shaderCompileResult;

        void createShaderModule();
        [[nodiscard]] auto compileShader(const std::string& shaderName, const std::string& shader_source) const -> ShaderCompileResult;
        [[nodiscard]] static auto readFile(const std::string& filepath) -> std::string;

    public:
        explicit ShaderModule(const dp::Context& context, std::string  name, dp::ShaderStage shaderStage);

        void createShader(const std::string& filename);
        [[nodiscard]] auto getShaderStageCreateInfo() const -> VkPipelineShaderStageCreateInfo;
        [[nodiscard]] auto getShaderStage() const -> dp::ShaderStage;
        [[nodiscard]] auto getHandle() const -> VkShaderModule;
    };
} // namespace dp
