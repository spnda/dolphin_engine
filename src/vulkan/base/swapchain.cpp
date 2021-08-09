#include "VkBootstrap.h"

#include "swapchain.hpp"
#include "../utils.hpp"

namespace dp {

bool VulkanSwapchain::create(vkb::Device device) {
    vkb::SwapchainBuilder swapchainBuilder(device);
    auto buildResult = swapchainBuilder
		.set_old_swapchain(this->swapchain)
		.set_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT) // We need this when switching layouts to copy the storage image.
		.build();
    vkb::destroy_swapchain(this->swapchain);
    swapchain = getFromVkbResult(buildResult);
    return true;
}

void VulkanSwapchain::destroy() {
    ctx.waitForIdle();
    
    vkDestroySurfaceKHR(ctx.instance.instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
}

VkResult VulkanSwapchain::aquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex) const {
    return fnAcquireNextImage(ctx.device.device, swapchain.swapchain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
}

VkResult VulkanSwapchain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) const {
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

std::vector<VkImage> VulkanSwapchain::getImages() {
    return getFromVkbResult(swapchain.get_images());
}

std::vector<VkImageView> VulkanSwapchain::getImageViews() {
    return getFromVkbResult(swapchain.get_image_views());
}

} // namespace dp
