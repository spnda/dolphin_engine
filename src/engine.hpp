#pragma once

#include "models/modelmanager.hpp"
#include "render/camera.hpp"
#include "render/ui.hpp"
#include "vulkan/base/swapchain.hpp"
#include "vulkan/resource/storageimage.hpp"
#include "vulkan/rt/rt_pipeline.hpp"
#include "options.hpp"

namespace dp {
    class Engine {
        const VkImageSubresourceRange defaultSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        dp::Context& ctx;

        dp::StorageImage storageImage;
        dp::Swapchain swapchain;
        dp::RayTracingPipeline pipeline;

        dp::ShaderModule rayGenShader;
        dp::ShaderModule rayMissShader;
        dp::ShaderModule shadowMissShader;
        dp::ShaderModule closestHitShader;
        dp::ShaderModule anyHitShader;

        dp::Buffer shaderBindingTable;
        VkStridedDeviceAddressRegionKHR raygenRegion = {};
        VkStridedDeviceAddressRegionKHR missRegion = {};
        VkStridedDeviceAddressRegionKHR chitRegion = {};
        VkStridedDeviceAddressRegionKHR callableRegion = {};
        uint32_t sbtStride = 0;

        struct PushConstants {
            glm::vec3 lightPosition = glm::vec3();
            float lightIntensity = 0.5f;
        } pushConstants;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        };

        void getProperties();
        void buildPipeline();
        void buildSBT();

    public:
        dp::Camera camera;
        dp::EngineOptions options = {};
        dp::ModelManager modelManager;
        dp::Ui ui;

        bool needsResize = false;

        explicit Engine(dp::Context& ctx);

        void renderLoop();
        void resize(uint32_t width, uint32_t height);
        void updateTlas();
        PushConstants& getConstants();
    };
}
