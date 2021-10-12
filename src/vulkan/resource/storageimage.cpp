#include "storageimage.hpp"

dp::StorageImage::StorageImage(const dp::Context& context)
        : ctx(context), image(ctx, { ctx.width, ctx.height }) {
    image.create(imageFormat, imageUsage, VK_IMAGE_LAYOUT_UNDEFINED);
    image.setName("storageImage");
}

dp::StorageImage::operator dp::Image() const {
    return image;
}

dp::StorageImage::operator VkImage() const {
    return image;
}

VkDescriptorImageInfo dp::StorageImage::getDescriptorImageInfo() const {
    return {
        .imageView = image.getImageView(),
        .imageLayout = image.getImageLayout(),
    };
}

VkImageLayout dp::StorageImage::getCurrentLayout() const {
    return image.getImageLayout();
}

VkExtent2D dp::StorageImage::getImageSize() const {
    return image.getImageSize();
}

VkExtent3D dp::StorageImage::getImageSize3d() const {
    return {
        .width = getImageSize().width,
        .height = getImageSize().height,
        .depth = 1,
    };
}

void dp::StorageImage::recreateImage() {
    image.destroy();
    dp::Image newImage(ctx, { ctx.width, ctx.height });
    newImage.create(imageFormat, imageUsage, VK_IMAGE_LAYOUT_UNDEFINED);
    image = newImage;
    image.setName("storageImage");
}

void dp::StorageImage::changeLayout(const VkCommandBuffer commandBuffer, const VkImageLayout newLayout) {
    image.changeLayout(commandBuffer, newLayout, subresourceRange);
}
