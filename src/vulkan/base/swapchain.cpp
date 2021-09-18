#include "VkBootstrap.h"

#include "swapchain.hpp"
#include "../utils.hpp"

bool dp::VulkanSwapchain::create(vkb::Device device) {
    vkb::SwapchainBuilder swapchainBuilder(device);
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

void dp::VulkanSwapchain::destroy() {
    ctx.waitForIdle();

	swapchain.destroy_image_views(views);
    vkb::destroy_swapchain(this->swapchain);
    
    vkDestroySurfaceKHR(ctx.instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
}

VkResult dp::VulkanSwapchain::aquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex) const {
    return fnAcquireNextImage(ctx.device, swapchain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
}

VkResult dp::VulkanSwapchain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore& waitSemaphore) const {
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain.swapchain;
	presentInfo.pImageIndices = &imageIndex;
    
	if (waitSemaphore != VK_NULL_HANDLE) {
		presentInfo.pWaitSemaphores = &waitSemaphore;
		presentInfo.waitSemaphoreCount = 1;
	}
	return fnQueuePresent(queue, &presentInfo);
}

VkResult dp::VulkanSwapchain::queuePresent(VkQueue queue, VkPresentInfoKHR* presentInfo) const {
	return fnQueuePresent(queue, presentInfo);
}

VkFormat dp::VulkanSwapchain::getFormat() const {
	return swapchain.image_format;
}

VkExtent2D dp::VulkanSwapchain::getExtent() const {
	return swapchain.extent;
}

std::vector<VkImage> dp::VulkanSwapchain::getImages() {
    return getFromVkbResult(swapchain.get_images());
}

std::vector<VkImageView> dp::VulkanSwapchain::getImageViews() {
    return getFromVkbResult(swapchain.get_image_views());
}
