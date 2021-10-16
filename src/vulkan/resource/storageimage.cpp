#include "storageimage.hpp"

dp::StorageImage::StorageImage(const dp::Context& context)
        : dp::Image(context, { context.width, context.height }, "storageImage") {

}

VkDescriptorImageInfo dp::StorageImage::getDescriptorImageInfo() const {
    return {
        .imageView = getImageView(),
        .imageLayout = getImageLayout(),
    };
}

void dp::StorageImage::create() {
    dp::Image::create(imageFormat, imageUsage, VK_IMAGE_LAYOUT_UNDEFINED);
}

void dp::StorageImage::recreateImage() {
    destroy();
    imageExtent = { ctx.width, ctx.height };
    dp::Image::create(imageFormat, imageUsage, VK_IMAGE_LAYOUT_UNDEFINED);
}

void dp::StorageImage::changeLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout,
                                    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    dp::Image::changeLayout(commandBuffer, newLayout, subresourceRange, srcStage, dstStage);
}
