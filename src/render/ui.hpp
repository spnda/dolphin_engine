#pragma once

#include "../vulkan/base/device.hpp"
#include "../vulkan/base/renderpass.hpp"
#include "../vulkan/context.hpp"
#include "../vulkan/utils.hpp"

namespace dp {
    class Ui {
        const dp::Context& ctx;
        const dp::VulkanSwapchain& swapchain;
        dp::RenderPass renderPass;
        VkDescriptorPool descriptorPool;
        std::vector<VkFramebuffer> framebuffers;

        void initFramebuffers();
    public:
        Ui(const dp::Context& context, const dp::VulkanSwapchain& vkSwapchain);
        ~Ui();

        void init();

        void prepare();
        void draw(const VkCommandBuffer cmdBuffer);

        static bool isInputting();
    };
}
