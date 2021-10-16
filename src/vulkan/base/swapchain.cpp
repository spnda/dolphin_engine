#include "VkBootstrap.h"

#include "swapchain.hpp"
#include "../utils.hpp"

dp::Swapchain::Swapchain(const Context& context, const VkSurfaceKHR surface)
        : ctx(context), surface(surface) {
    create(ctx.device);
    vkAcquireNextImage = ctx.device.getFunctionAddress<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR");
    vkDestroySurface = ctx.device.getFunctionAddress<PFN_vkDestroySurfaceKHR>("vkDestroySurfaceKHR");
}

bool dp::Swapchain::create(const dp::Device& device) {
    vkb::SwapchainBuilder swapchainBuilder((vkb::Device(device)));
    auto buildResult = swapchainBuilder
        .set_old_swapchain(this->swapchain)
        .set_desired_extent(ctx.width, ctx.height)
        .set_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) // We need this when switching layouts to copy the storage image.
        .build();
    vkb::destroy_swapchain(this->swapchain);
    swapchain = getFromVkbResult(buildResult);
    images = getImages();
    views = getImageViews();
    return true;
}

void dp::Swapchain::destroy() {
    auto result = ctx.device.waitIdle();
    checkResult(ctx, result, "Failed waiting on device idle");

    swapchain.destroy_image_views(views);
    vkb::destroy_swapchain(this->swapchain);
    
    vkDestroySurface(ctx.instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
}

VkResult dp::Swapchain::acquireNextImage(const dp::Semaphore& presentCompleteSemaphore, uint32_t* imageIndex) const {
    return vkAcquireNextImage(ctx.device, swapchain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
}

VkResult dp::Swapchain::queuePresent(dp::Queue& queue, uint32_t imageIndex, dp::Semaphore& waitSemaphore) const {
    return queue.present(imageIndex, swapchain.swapchain, waitSemaphore);
}

VkFormat dp::Swapchain::getFormat() const {
    return swapchain.image_format;
}

VkExtent2D dp::Swapchain::getExtent() const {
    return swapchain.extent;
}

std::vector<VkImage> dp::Swapchain::getImages() {
    return getFromVkbResult(swapchain.get_images());
}

std::vector<VkImageView> dp::Swapchain::getImageViews() {
    return getFromVkbResult(swapchain.get_image_views());
}
