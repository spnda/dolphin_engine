#pragma once

#include "../context.hpp"

namespace dp {

// fwd.
struct Context;

class Image {
    const Context& context;

    VmaAllocation allocation;

public:
    VkImage image;
    VkImageView imageView;

    const VkExtent2D imageExtent;

    Image(const Context& context, const VkExtent2D extent, const VkFormat format, const VkImageUsageFlags usageFlags, const VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

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
