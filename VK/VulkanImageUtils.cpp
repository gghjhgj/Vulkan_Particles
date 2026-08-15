#include "VulkanImageUtils.h"

#include <stdexcept>

namespace VulkanImageUtils
{

void transitionImageLayout(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier{};

    barrier.sType =
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    barrier.oldLayout =
        oldLayout;

    barrier.newLayout =
        newLayout;

    barrier.srcQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;

    barrier.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;

    barrier.image =
        image;

    barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;

    barrier.subresourceRange.baseMipLevel =
        0;

    barrier.subresourceRange.levelCount =
        1;

    barrier.subresourceRange.baseArrayLayer =
        0;

    barrier.subresourceRange.layerCount =
        1;

    VkPipelineStageFlags srcStage =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    VkPipelineStageFlags dstStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    if (
        oldLayout ==
            VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout ==
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask =
            0;

        barrier.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        srcStage =
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        dstStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    else if (
        oldLayout ==
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR &&
        newLayout ==
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask =
            0;

        barrier.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        srcStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        dstStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    else if (
        oldLayout ==
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
        newLayout ==
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        barrier.srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        barrier.dstAccessMask =
            0;

        srcStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        dstStage =
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    else
    {
        throw std::runtime_error(
            "Unsupported image layout transition."
        );
    }

    vkCmdPipelineBarrier(
        commandBuffer,

        srcStage,
        dstStage,

        0,

        0,
        nullptr,

        0,
        nullptr,

        1,
        &barrier
    );
}

}