#include "image.hpp"

dp::Image::Image(const Context& context, const VkExtent2D extent, std::string name)
        : ctx(context), imageExtent(extent), name(std::move(name)) {
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

    vmaCreateImage(ctx.vmaAllocator, &imageCreateInfo, &allocationInfo, &image, &allocation, nullptr);

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

    vkCreateImageView(ctx.device, &imageViewCreateInfo, nullptr, &imageView);

    ctx.setDebugUtilsName(image, name);
}

void dp::Image::copyImage(VkCommandBuffer cmdBuffer, VkImage destination, VkImageLayout destinationLayout) const {
    VkImageCopy copyRegion = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffset = { 0, 0, 0 },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffset = { 0, 0, 0 },
        .extent = getImageSize3d(),
    };
    vkCmdCopyImage(cmdBuffer, this->image, this->getImageLayout(), destination, destinationLayout, 1, &copyRegion);
}

void dp::Image::destroy() {
    vkDestroyImageView(ctx.device, imageView, nullptr);
    vmaDestroyImage(ctx.vmaAllocator, image, allocation);
    imageView = nullptr;
    image = nullptr;
}

VkImageView dp::Image::getImageView() const {
    return imageView;
}

VkExtent2D dp::Image::getImageSize() const {
    return imageExtent;
}

VkExtent3D dp::Image::getImageSize3d() const {
    return {
        .width = getImageSize().width,
        .height = getImageSize().height,
        .depth = 1,
    };
}

VkImageLayout dp::Image::getImageLayout() const {
    return currentLayout;
}

void dp::Image::changeLayout(
        VkCommandBuffer commandBuffer,
        const VkImageLayout newLayout,
        const VkImageSubresourceRange subresourceRange,
        const VkPipelineStageFlags srcStage, const VkPipelineStageFlags dstStage) {
    dp::Image::changeLayout(image, commandBuffer, currentLayout, newLayout, srcStage, dstStage, subresourceRange);
    currentLayout = newLayout;
}

void dp::Image::changeLayout(VkImage image, VkCommandBuffer commandBuffer,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                             VkImageSubresourceRange subresourceRange) {
    if (oldLayout == newLayout) return;

    uint32_t srcAccessMask, dstAccessMask;
    switch (srcStage) {
        default:
            srcAccessMask = 0;
            break;
        case VK_PIPELINE_STAGE_TRANSFER_BIT:
            srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR:
        case VK_ACCESS_SHADER_WRITE_BIT:
            srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
    }
    switch (dstStage) {
        default:
            dstAccessMask = 0;
            break;
        case VK_PIPELINE_STAGE_TRANSFER_BIT:
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
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
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageBarrier);
}
