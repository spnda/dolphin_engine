#include "image.hpp"

dp::Image::Image(const Context& context, const VkExtent2D extent)
        : context(context), imageExtent(extent) {
}

dp::Image::operator VkImage() const {
    return this->image;
}

dp::Image& dp::Image::operator=(const dp::Image& newImage) {
    if (this == &newImage) return *this;
    this->image = newImage.image;
    this->imageExtent = newImage.imageExtent;
    this->imageView = newImage.imageView;
    this->allocation = newImage.allocation;
    return *this;
}

void dp::Image::create(const VkFormat format, const VkImageUsageFlags usageFlags, const VkImageLayout initialLayout) {
    currentLayout = initialLayout;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = nullptr;

    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = { imageExtent.width, imageExtent.height, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usageFlags;
    imageCreateInfo.initialLayout = currentLayout;

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(context.vmaAllocator, &imageCreateInfo, &allocationInfo, &image, &allocation, nullptr);

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = nullptr;

    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCreateImageView(context.device, &imageViewCreateInfo, nullptr, &imageView);
}

void dp::Image::destroy() {
    vkDestroyImageView(context.device, imageView, nullptr);
    vmaDestroyImage(context.vmaAllocator, image, allocation);
    imageView = nullptr;
    image = nullptr;
}

void dp::Image::free() {
    vkDestroyImageView(context.device, imageView, nullptr);
    vmaFreeMemory(context.vmaAllocator, allocation);
    imageView = nullptr;
    allocation = nullptr;
}

void dp::Image::setName(const std::string& name) {
    context.setDebugUtilsName(image, name);
}

VkImageView dp::Image::getImageView() const {
    return imageView;
}

VkExtent2D dp::Image::getImageSize() const {
    return imageExtent;
}

VkImageLayout dp::Image::getImageLayout() const {
    return currentLayout;
}

void dp::Image::changeLayout(
        const VkCommandBuffer commandBuffer,
        const VkImageLayout newLayout,
        const VkImageSubresourceRange subresourceRange) {
    dp::Image::changeLayout(image, commandBuffer, currentLayout, newLayout, subresourceRange);
    currentLayout = newLayout;
}

void dp::Image::changeLayout(VkImage image, VkCommandBuffer commandBuffer, VkImageLayout oldLayout,
                             VkImageLayout newLayout, VkImageSubresourceRange subresourceRange) {
    if (oldLayout == newLayout) return;

    uint32_t srcAccessMask = 0, dstAccessMask = 0;
    switch (oldLayout) {
        case VK_IMAGE_LAYOUT_GENERAL:
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
    }
    switch (newLayout) {
        case VK_IMAGE_LAYOUT_GENERAL:
        case VK_IMAGE_LAYOUT_UNDEFINED:
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        default:
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;
    }

    VkImageMemoryBarrier imageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresourceRange,
    };

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageBarrier);
}
