#include "VulkanFrameData.h"
#include "VulkanContext.h"
#include <stdexcept>

void VulkanFrameData::init(const VulkanContext& context)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context.graphicsQueueFamilyIndex;

    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Renderer Error: Failed to create per-frame VkCommandPool.");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Renderer Error: Failed to allocate per-frame VkCommandBuffer.");
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(context.device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Renderer Error: Failed to create synchronization primitives per frame.");
    }
}

void VulkanFrameData::destroy(VkDevice device)
{
    if (device == VK_NULL_HANDLE) return;

    if (inFlightFence != VK_NULL_HANDLE) vkDestroyFence(device, inFlightFence, nullptr);
    if (renderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    if (imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }
}