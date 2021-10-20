#include "texture.hpp"

#include <utility>

#include "../utils.hpp"

dp::Texture::Texture(const dp::Context& context, const VkExtent2D imageSize, std::string name)
    : dp::Image(context, imageSize, std::move(name)) {

}

dp::Texture::operator VkImageView() const {
    return getImageView();
}

dp::Texture& dp::Texture::operator=(const Texture& newImage) {
    sampler = newImage.sampler;
    name = newImage.name;
    dp::Image::operator=(newImage);
    return *this;
}

void dp::Texture::createTexture(VkFormat format) {
    dp::Image::create(format, imageUsage, VK_IMAGE_LAYOUT_UNDEFINED);

    VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,

        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxLod = VK_LOD_CLAMP_NONE,
    };
    auto result = vkCreateSampler(ctx.device, &samplerCreateInfo, nullptr, &sampler);
    checkResult(ctx, result, "Failed to create sampler");
}

VkSampler dp::Texture::getSampler() const {
    return sampler;
}

void dp::Texture::changeLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout,
                                    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    dp::Image::changeLayout(commandBuffer, newLayout, subresourceRange, srcStage, dstStage);
}
