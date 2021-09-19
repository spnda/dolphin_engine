#pragma once

#include <vector>

#include "../vulkan/base/renderpass.hpp"

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

        void init();
        void destroy();

        void prepare();
        void draw(const VkCommandBuffer cmdBuffer);

        void resize();

        static bool isInputting();
    };
}
