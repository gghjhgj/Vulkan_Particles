#include "VulkanIndirectBuffer.h"
#include "VulkanContext.h"
#include <stdexcept>

void VulkanIndirectBuffer::init(const VulkanContext& context, uint32_t maxCommands)
{
    this->maxDrawCommands = maxCommands;

    VkDeviceSize bufferSize = maxCommands * sizeof(VkDrawIndirectCommand);

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | 
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    gpuBuffer.init(context, bufferSize, usage, properties, false);
}

void VulkanIndirectBuffer::reset(VkCommandBuffer commandBuffer)
{
    vkCmdFillBuffer(commandBuffer, gpuBuffer.handle, 0, VK_WHOLE_SIZE, 0);
}

VkDescriptorBufferInfo VulkanIndirectBuffer::getDescriptorInfo() const
{
    VkDescriptorBufferInfo info{};
    info.buffer = gpuBuffer.handle;
    info.offset = 0;
    info.range = maxDrawCommands * sizeof(VkDrawIndirectCommand);
    return info;
}

void VulkanIndirectBuffer::destroy(VkDevice device)
{
    gpuBuffer.destroy(device);
}