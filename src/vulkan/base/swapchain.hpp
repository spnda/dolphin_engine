#pragma once

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

#include "../context.hpp"

namespace dp {
    class Swapchain {
        dp::Context& ctx;

        VkSurfaceKHR surface;

        PFN_vkAcquireNextImageKHR vkAcquireNextImage;
        PFN_vkQueuePresentKHR vkQueuePresent;
        PFN_vkDestroySurfaceKHR vkDestroySurface;

        std::vector<VkImage> getImages();
        std::vector<VkImageView> getImageViews();
        
    public:
        vkb::Swapchain swapchain = {};

        std::vector<VkImage> images = {};
        std::vector<VkImageView> views = {};

        Swapchain(Context& context, VkSurfaceKHR surface) : ctx(context), surface(surface) {
            this->create(ctx.device);
            this->vkAcquireNextImage = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(context.device, "vkAcquireNextImageKHR"));
            this->vkQueuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(context.device, "vkQueuePresentKHR"));
            this->vkDestroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(vkGetInstanceProcAddr(context.instance, "vkDestroySurfaceKHR"));
        }

        // Creates a new swapchain.
        // If a swapchain already exists, we re-use it and
        // later destroy it.
        bool create(const vkb::Device& device);

        void destroy();

        VkResult aquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex) const;

        VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore& waitSemaphore) const;

        VkResult queuePresent(VkQueue, VkPresentInfoKHR* presentInfo) const;

        [[nodiscard]] VkFormat getFormat() const;
        [[nodiscard]] VkExtent2D getExtent() const;
    };
} // namespace dp
