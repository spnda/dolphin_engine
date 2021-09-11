#pragma once

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

#include "../context.hpp"
#include "surface.hpp"

namespace dp {

class VulkanSwapchain {
private:
    dp::Context& ctx;

    VkSurfaceKHR surface;

    PFN_vkAcquireNextImageKHR fnAcquireNextImage;
    PFN_vkQueuePresentKHR fnQueuePresent;

    std::vector<VkImage> getImages();
    std::vector<VkImageView> getImageViews();
    
public:
    vkb::Swapchain swapchain = {};

    std::vector<VkImage> images = {};
    std::vector<VkImageView> views = {};

    VulkanSwapchain(Context& context, VkSurfaceKHR surface) : ctx(context), surface(surface) {
        this->create(ctx.device);
        this->fnAcquireNextImage = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(context.device, "vkAcquireNextImageKHR"));
        this->fnQueuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(context.device, "vkQueuePresentKHR"));
    }

    // Creates a new swapchain.
    // If this->swapchain already exists, we use it as a base for
    // re-creation.
    bool create(vkb::Device device);

    void destroy();

    VkResult aquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex) const;

    VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore& waitSemaphore) const;

    VkResult queuePresent(VkQueue, VkPresentInfoKHR* presentInfo) const;

    VkFormat getFormat() const;
    VkExtent2D getExtent() const;
};

} // namespace dp
