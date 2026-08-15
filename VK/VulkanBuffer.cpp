#include "VulkanBuffer.h"
#include "VulkanContext.h"

void VulkanBuffer::init(const VulkanContext& context, 
                        VkDeviceSize bufferSize, 
                        VkBufferUsageFlags usage, 
                        VkMemoryPropertyFlags properties,
                        bool mapMemory) 
{
    this->size = bufferSize;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(context.device, &bufferInfo, nullptr, &handle) != VK_SUCCESS) {
        throw std::runtime_error("couldn't create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context.device, handle, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context.physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(context.device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("couldn't allocate memory for buffer");
    }

    vkBindBufferMemory(context.device, handle, memory, 0);

    if (mapMemory) {
        if (vkMapMemory(context.device, memory, 0, bufferSize, 0, &mappedData) != VK_SUCCESS) {
            throw std::runtime_error("couldn't map memory");
        }
        std::cout << "Storage buffer size: " << bufferSize << " bytes allocated & mapped" << std::endl;
    }
}

void VulkanBuffer::upload(const void* data, VkDeviceSize dataSize) {
    if (mappedData == nullptr) {
        throw std::runtime_error("mappedData buffer isn't mapped");
    }

    if (dataSize > size) {
        throw std::runtime_error("error: world grid size is bigger than declared on GPU buffer");
    }

    std::memcpy(mappedData, data, dataSize);
}

void VulkanBuffer::download(void* dest, VkDeviceSize size)
{
    if(!mappedData)
    {
        std::cerr << "error: buffer not mapped" << std::endl;
        return;
    }
    std::memcpy(dest, mappedData, size);
}


void VulkanBuffer::destroy(VkDevice device) {
    if (mappedData != nullptr && device != VK_NULL_HANDLE) {
        vkUnmapMemory(device, memory);
        mappedData = nullptr;
    }
    if (handle != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

uint32_t VulkanBuffer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("couldn't find correct memory type");
}