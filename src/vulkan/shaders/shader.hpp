#pragma once

#include <map>

#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp>

namespace dp {
    enum class ShaderStage : uint64_t {
        RayGeneration = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        RayMiss = VK_SHADER_STAGE_MISS_BIT_KHR
    };

    inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
        return static_cast<ShaderStage>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
    }

    inline bool operator>(ShaderStage a, ShaderStage b) {
        return static_cast<uint64_t>(a) > static_cast<uint64_t>(b);
    }

    // fwd.
    class Context;

    class ShaderModule {
        const dp::Context& ctx;
        std::string name;

        VkShaderModule shaderModule = nullptr;
        dp::ShaderStage shaderStage;

        std::vector<uint32_t> shaderBinary;

        void createShaderModule();
        [[nodiscard]] auto compileShader(const std::string& shaderName, const std::string& shader_source) const -> std::vector<uint32_t>;
        [[nodiscard]] static auto readFile(const std::string& filepath) -> std::string;

    public:
        explicit ShaderModule(const dp::Context& context, std::string  name, dp::ShaderStage shaderStage);

        void createShader(const std::string& filename);
        [[nodiscard]] auto getShaderStageCreateInfo() const -> VkPipelineShaderStageCreateInfo;
        [[nodiscard]] auto getShaderStage() const -> dp::ShaderStage;
        [[nodiscard]] auto getHandle() const -> VkShaderModule;
    };
} // namespace dp
