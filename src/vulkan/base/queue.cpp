#include "queue.hpp"

#include <utility>

#include "../context.hpp"

dp::Queue::Queue(const dp::Context& context, std::string name)
    : ctx(context), name(std::move(name)) {
}

dp::Queue::Queue(const dp::Queue& queue)
    : ctx(queue.ctx), handle(queue.handle), name(queue.name) {
}

dp::Queue::operator VkQueue() const {
    return this->handle;
}

void dp::Queue::create(const vkb::QueueType queueType) {
    vkQueuePresent = ctx.device.getFunctionAddress<PFN_vkQueuePresentKHR>("vkQueuePresentKHR");
    handle = ctx.device.getQueue(queueType);

    if (!name.empty())
        ctx.setDebugUtilsName(handle, name);
}

void dp::Queue::lock() const {
    queueMutex.lock();
}

void dp::Queue::unlock() const {
    queueMutex.unlock();
}

void dp::Queue::waitIdle() const {
    vkQueueWaitIdle(handle);
}

std::unique_lock<std::mutex> dp::Queue::getLock() const {
    return std::unique_lock(queueMutex); // Auto locks.
}

VkResult dp::Queue::submit(const dp::Fence& fence, const VkSubmitInfo* submitInfo) const {
    return vkQueueSubmit(handle, 1, submitInfo, fence);
}

VkResult dp::Queue::present(uint32_t imageIndex, const VkSwapchainKHR& swapchain, const dp::Semaphore& waitSemaphore) const {
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    };
    if (waitSemaphore != nullptr) {
        presentInfo.pWaitSemaphores = &waitSemaphore.getHandle();
        presentInfo.waitSemaphoreCount = 1;
    }
    return vkQueuePresent(handle, &presentInfo);
}
