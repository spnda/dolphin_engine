#pragma once

#include <vector>

#include "../vulkan/base/renderpass.hpp"

namespace dp {
    class Engine;

    class Ui {
        const dp::Context& ctx;
        const dp::Swapchain& swapchain;
        dp::RenderPass renderPass;
        VkDescriptorPool descriptorPool = nullptr;
        std::vector<VkFramebuffer> framebuffers;

        void initFramebuffers();

    public:
        Ui(const dp::Context& context, const dp::Swapchain& vkSwapchain);

        void init();
        void destroy();

        void prepare();
        void draw(dp::Engine& engine, VkCommandBuffer cmdBuffer);

        void resize();

        static bool isInputting();
    };
}
