#include "storageimage.hpp"

dp::StorageImage::StorageImage(const dp::Context& context)
        : ctx(context), image(ctx, { ctx.width, ctx.height }, imageFormat, imageUsage) {
    image.setName("storageImage");
}

dp::StorageImage::operator dp::Image() const {
    return image;
}

dp::StorageImage::operator VkImage() const {
    return image.image;
}

VkDescriptorImageInfo dp::StorageImage::getDescriptorImageInfo() const {
    return {
        .imageView = image.imageView,
        .imageLayout = currentLayout,
    };
}

const VkImageLayout dp::StorageImage::getCurrentLayout() const {
    return currentLayout;
}

void dp::StorageImage::recreateImage() {
    image.free();
    currentLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Reset the image layout
    dp::Image newImage(ctx, { ctx.width, ctx.height }, imageFormat, imageUsage);
    image = newImage;
    image.setName("storageImage");
}

void dp::StorageImage::changeLayout(const VkCommandBuffer commandBuffer, const VkImageLayout newLayout) {
    if (currentLayout == newLayout) return;

    uint32_t srcAccessMask = 0, dstAccessMask = 0;

    switch (currentLayout) {
        case VK_IMAGE_LAYOUT_GENERAL:
        case VK_IMAGE_LAYOUT_UNDEFINED:
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
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;
    }

    dp::Image::changeLayout(
        image, commandBuffer,
        currentLayout, newLayout,
        srcAccessMask, dstAccessMask,
        subresourceRange
    );
    currentLayout = newLayout;
}
