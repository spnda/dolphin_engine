#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace dp {
    // fwd
    class Context;
    class Swapchain;

    class RenderPass {
        const dp::Context& ctx;
        const dp::Swapchain& swapchain;

        VkRenderPass handle = VK_NULL_HANDLE;

    public:
        RenderPass(const dp::Context& context, const dp::Swapchain& swapchain);

        void create(VkAttachmentLoadOp colorBufferLoadOp, const std::string& name = {});
        void destroy();
        void begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer, std::vector<VkClearValue> clearValues = {});
        void end(VkCommandBuffer cmdBuffer);

        explicit operator VkRenderPass() const;
    };
}
