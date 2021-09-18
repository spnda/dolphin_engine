#pragma once

#include "render/camera.hpp"
#include "render/modelloader.hpp"
#include "render/ui.hpp"
#include "vulkan/base/swapchain.hpp"
#include "vulkan/resource/storageimage.hpp"
#include "vulkan/rt/acceleration_structure_builder.hpp"
#include "vulkan/rt/rt_pipeline.hpp"
#include "vulkan/context.hpp"

namespace dp {
    class Engine {
        const VkImageSubresourceRange defaultSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        dp::Context& ctx;
        dp::ModelLoader modelLoader;
        dp::Camera camera;
        dp::Ui ui;

        dp::StorageImage storageImage;
        dp::VulkanSwapchain swapchain;
        dp::RayTracingPipeline pipeline;
        dp::AccelerationStructure topLevelAccelerationStructure;

        dp::Buffer raygenShaderBindingTable;
        dp::Buffer missShaderBindingTable;
        dp::Buffer hitShaderBindingTable;
        uint32_t sbtStride;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        };

        void getProperties();
        void buildPipeline();
        void buildSBT();
    public:
        Engine(dp::Context& ctx);

        void renderLoop();
    };
}
