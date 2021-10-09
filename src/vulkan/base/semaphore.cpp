#include "semaphore.hpp"

#include "../context.hpp"

dp::Semaphore::Semaphore(const dp::Context& context, std::string name)
        : ctx(context), name(std::move(name)) {

}

dp::Semaphore::operator VkSemaphore() const {
    return handle;
}

void dp::Semaphore::create(VkSemaphoreCreateFlags flags) {
    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .flags = flags,
    };
    vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr, &handle);
}

void dp::Semaphore::destroy() const {
    vkDestroySemaphore(ctx.device, handle, nullptr);
}

auto dp::Semaphore::getHandle() const -> const VkSemaphore& {
    return handle;
}
