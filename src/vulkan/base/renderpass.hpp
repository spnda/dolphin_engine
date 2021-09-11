#pragma once

#include "../context.hpp"
#include "swapchain.hpp"

namespace dp {
    class RenderPass {
        const dp::Context& ctx;
        const dp::VulkanSwapchain& swapchain;

        VkRenderPass handle = VK_NULL_HANDLE;

    public:
        RenderPass(const dp::Context& context, const dp::VulkanSwapchain& swapchain);

        void create(const VkAttachmentLoadOp colorBufferLoadOp, const std::string name = {});
        void destroy();
        void begin(const VkCommandBuffer cmdBuffer, const VkFramebuffer framebuffer, std::vector<VkClearValue> clearValues = {});
        void end(const VkCommandBuffer cmdBuffer);

        operator VkRenderPass() const;
    };
}
