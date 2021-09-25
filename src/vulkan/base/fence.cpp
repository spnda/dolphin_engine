#include "fence.hpp"

#include "../context.hpp"

#define DEFAULT_FENCE_TIMEOUT 100000000000

dp::Fence::Fence(const dp::Context& context, const std::string name)
    : ctx(context), name(name) {
}

dp::Fence::Fence(const dp::Fence& fence)
    : ctx(fence.ctx), handle(fence.handle), name(fence.name) {
}

dp::Fence::operator VkFence() const {
    return handle;
}

void dp::Fence::create(const VkFenceCreateFlags flags) {
    VkFenceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = flags,
    };
    vkCreateFence(ctx.device, &info, nullptr, &handle);
    assert(handle != nullptr);
    if (!name.empty()) ctx.setDebugUtilsName(handle, name);
}

void dp::Fence::destroy() {
    vkDestroyFence(ctx.device, handle, nullptr);
}

void dp::Fence::wait() {
    // Waiting on fences is totally thread safe.
    vkWaitForFences(ctx.device, 1, &handle, true, DEFAULT_FENCE_TIMEOUT);
}

void dp::Fence::reset() {
    waitMutex.lock();
    vkResetFences(ctx.device, 1, &handle);
    waitMutex.unlock();
}
