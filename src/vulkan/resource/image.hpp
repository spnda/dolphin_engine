#pragma once

#include <string>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "../context.hpp"

namespace dp {
    class Image {
        const Context& context;

        VmaAllocation allocation;

    public:
        VkImage image;
        VkImageView imageView;

        VkExtent2D imageExtent;

        Image(const Context& context, const VkExtent2D extent, const VkFormat format, const VkImageUsageFlags usageFlags, const VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

        operator VkImage() const;
        Image& operator=(const dp::Image& newImage);

        /** Destroys the image view, frees all memory and destroys the image. */
        void destroy();
        /** Destroys the image view and frees all memory, but does not destroy the image. */
        void free();

        void setName(const std::string name);

        static void changeLayout(
            const VkImage image,
            const VkCommandBuffer commandBuffer,
            const VkImageLayout oldLayout,
            const VkImageLayout newLayout,
            const VkAccessFlags srcAccessMask,
            const VkAccessFlags dstAccessMask,
            const VkImageSubresourceRange subresourceRange);
    };
}
