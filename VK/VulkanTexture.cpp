#include "VulkanTexture.h"
#include "VulkanContext.h"

#include <stdexcept>

void VulkanTexture::init(
    VulkanContext& context,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageUsageFlags additionalUsage)
{
    this->width = width;
    this->height = height;
    this->format = format;
    this->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // 1. Tworzenie obiektu VkImage
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    // Domyślnie dajemy uprawnienia do zapisu/odczytu w compute + próbkowania + transferu
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | 
                      VK_IMAGE_USAGE_SAMPLED_BIT | 
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
                      additionalUsage;
                      
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(context.device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("Nie udalo sie utworzyc VkImage w VulkanTexture");
    }

    // 2. Alokacja pamięci w VRAM (DEVICE_LOCAL)
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(context.device, image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = context.findMemoryType(
        memReqs.memoryTypeBits, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(context.device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
    {
        throw std::runtime_error("Nie udalo sie zaalokowac pamieci dla VulkanTexture");
    }

    vkBindImageMemory(context.device, image, memory, 0);

    // 3. Tworzenie widoku VkImageView
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(context.device, &viewInfo, nullptr, &view) != VK_SUCCESS)
    {
        throw std::runtime_error("Nie udalo sie utworzyc VkImageView w VulkanTexture");
    }
}

void VulkanTexture::transitionLayout(
    VkCommandBuffer cmd,
    VkImageLayout newLayout,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage)
{
    if (currentLayout == newLayout)
        return;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = currentLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Automatyczne ustawianie masek dostępu w zależności od layoutu
    if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    else if (currentLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }
    else if (currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    else
    {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    currentLayout = newLayout;
}

VkSampler VulkanTexture::createLinearClampSampler(VkDevice device)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    VkSampler sampler{VK_NULL_HANDLE};
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Nie udalo sie utworzyc VkSampler dla VulkanTexture");
    }

    return sampler;
}

void VulkanTexture::destroy(VkDevice device)
{
    if (device == VK_NULL_HANDLE)
        return;

    if (view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    if (image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, image, nullptr);
        image = VK_NULL_HANDLE;
    }

    if (memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }

    currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}