#include "texture.hpp"

#include "../utils.hpp"

dp::Texture::Texture(const dp::Context& context, const VkExtent2D imageSize, std::string name)
    : ctx(context), image(ctx, imageSize), name(std::move(name)) {

}

dp::Texture::operator dp::Image() const {
    return image;
}

dp::Texture::operator VkImage() const {
    return image;
}

dp::Texture::operator VkImageView() const {
    return image.getImageView();
}

void dp::Texture::create() {
    image.create(imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
    image.setName(name);

    VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,

        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    };
    auto result = vkCreateSampler(ctx.device, &samplerCreateInfo, nullptr, &sampler);
    checkResult(ctx, result, "Failed to create sampler");
}

void dp::Texture::changeLayout(const VkCommandBuffer commandBuffer, const VkImageLayout newLayout) {
    image.changeLayout(commandBuffer, newLayout, subresourceRange);
}

VkSampler dp::Texture::getSampler() const {
    return sampler;
}

VkExtent2D dp::Texture::getSize() const {
    return image.getImageSize();
}

VkImageLayout dp::Texture::getCurrentLayout() const {
    return image.getImageLayout();
}
