#pragma once

#include <vulkan/vulkan.h>

class VulkanContext;

struct VulkanFrameData
{
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;

    VkFence inFlightFence = VK_NULL_HANDLE;

    void init(const VulkanContext& context);
    void destroy(VkDevice device);
};