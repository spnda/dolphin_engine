#pragma once

#include <string>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "../context.hpp"

namespace dp {
    class Image {
        const Context& context;

        VmaAllocation allocation = nullptr;
        VkImage image = nullptr;
        VkImageView imageView = nullptr;
        VkExtent2D imageExtent = {0, 0};
        VkFormat format;
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    public:
        Image(const Context& context, VkExtent2D extent);

        operator VkImage() const;
        Image& operator=(const Image& newImage);

        void create(VkFormat format, VkImageUsageFlags usageFlags, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);
        /** Destroys the image view, frees all memory and destroys the image. */
        void destroy();
        /** Destroys the image view and frees all memory, but does not destroy the image. */
        void free();

        void setName(const std::string& name);

        [[nodiscard]] VkImageView getImageView() const;
        [[nodiscard]] VkExtent2D getImageSize() const;
        [[nodiscard]] VkImageLayout getImageLayout() const;

        void changeLayout(
            VkCommandBuffer commandBuffer,
            VkImageLayout newLayout,
            VkImageSubresourceRange subresourceRange);

        static void changeLayout(
            VkImage image,
            VkCommandBuffer commandBuffer,
            VkImageLayout oldLayout, VkImageLayout newLayout,
            VkImageSubresourceRange subresourceRange
        );
    };
}
