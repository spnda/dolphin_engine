#include "queue.hpp"

#include "../context.hpp"
#include "../utils.hpp"
#include "fence.hpp"

dp::Queue::Queue(const dp::Context& context, const std::string name)
    : ctx(context), name(name) {
}

dp::Queue::Queue(const dp::Context& context, const VkQueue queue)
        : ctx(context), handle(queue), name({}) {
}

dp::Queue::Queue(const dp::Queue& queue)
    : ctx(queue.ctx), handle(queue.handle), name(queue.name) {
}

dp::Queue::operator VkQueue() const {
    return this->handle;
}

void dp::Queue::getQueue(const vkb::QueueType queueType) {
    handle = getFromVkbResult(ctx.device.get_queue(queueType));
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

auto dp::Queue::submit(const dp::Fence& fence, const VkSubmitInfo* submitInfo) const -> VkResult {
    return vkQueueSubmit(handle, 1, submitInfo, VkFence(fence));
}
