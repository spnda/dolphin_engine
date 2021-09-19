#include "image.hpp"

#include "../context.hpp"

dp::Image::Image(const Context& context, const VkExtent2D extent, const VkFormat format, const VkImageUsageFlags usageFlags, const VkImageLayout initialLayout)
        : context(context), imageExtent(extent) {
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
    imageCreateInfo.initialLayout = initialLayout;

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

void dp::Image::destroy() {
    vkDestroyImageView(context.device, imageView, nullptr);
    vmaDestroyImage(context.vmaAllocator, image, allocation);
    image = VK_NULL_HANDLE;
}

void dp::Image::free() {
    vkDestroyImageView(context.device, imageView, nullptr);
    vmaFreeMemory(context.vmaAllocator, allocation);
}

void dp::Image::setName(const std::string name) {
    context.setDebugUtilsName(image, name);
}

void dp::Image::changeLayout(
        const VkImage image,
        const VkCommandBuffer commandBuffer,
        const VkImageLayout oldLayout,
        const VkImageLayout newLayout,
        const VkAccessFlags srcAccessMask,
        const VkAccessFlags dstAccessMask,
        const VkImageSubresourceRange subresourceRange) {
    VkImageMemoryBarrier imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.image = image;
    imageBarrier.subresourceRange = subresourceRange;
    imageBarrier.oldLayout = oldLayout;
    imageBarrier.newLayout = newLayout;

    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstAccessMask = dstAccessMask;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageBarrier);
}
