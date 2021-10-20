#pragma once

#include <vulkan/vulkan.h>

#include "image.hpp"

namespace dp {
    class Context;

    class Texture : public Image {
        static const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; // This is the format STB uses.
        const VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        std::string name;
        VkSampler sampler = nullptr;

    public:
        static const VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        Texture(const dp::Context& context, VkExtent2D imageSize, std::string name = "texture");

        explicit operator VkImageView() const;
        Texture& operator=(const Texture& newImage);

        void createTexture(VkFormat newFormat = VK_FORMAT_R8G8B8A8_SRGB);
        void changeLayout(
            VkCommandBuffer commandBuffer,
            VkImageLayout newLayout,
            VkPipelineStageFlags srcStage,
            VkPipelineStageFlags dstStage);

        [[nodiscard]] VkSampler getSampler() const;
    };
}
