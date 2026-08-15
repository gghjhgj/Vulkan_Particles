#pragma once

#include <vulkan/vulkan.h>

namespace VulkanImageUtils
{
    void transitionImageLayout(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );
}