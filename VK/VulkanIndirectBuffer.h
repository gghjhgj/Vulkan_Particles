#pragma once

#include <vulkan/vulkan.h>
#include "VulkanBuffer.h"

class VulkanContext;

class VulkanIndirectBuffer
{
public:
    VulkanBuffer gpuBuffer;
    uint32_t maxDrawCommands = 0;

    void init(const VulkanContext& context, uint32_t maxCommands);
    
    void reset(VkCommandBuffer commandBuffer);

    void destroy(VkDevice device);

    VkDescriptorBufferInfo getDescriptorInfo() const;
};