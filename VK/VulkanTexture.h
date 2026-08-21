#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class VulkanContext;

class VulkanTexture
{
public:
    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkImageLayout currentLayout{VK_IMAGE_LAYOUT_UNDEFINED};

    uint32_t width{0};
    uint32_t height{0};

    VulkanTexture() = default;
    ~VulkanTexture() = default;

    // Inicjalizuje teksturę 2D jako Storage Image + Sampled Image
    void init(
        VulkanContext& context,
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageUsageFlags additionalUsage = 0);

    // Pomocnik do zmiany layoutu tekstury w Command Bufferze
    void transitionLayout(
        VkCommandBuffer cmd,
        VkImageLayout newLayout,
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    // Statyczny pomocnik do tworzenia samplera dwuliniowego z klamopwaniem do krawędzi
    static VkSampler createLinearClampSampler(VkDevice device);

    // Zwalnianie zasobów
    void destroy(VkDevice device);
};