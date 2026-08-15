#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <iostream>

class VulkanContext;

class VulkanSwapchain
{
public:
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};
    
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;

    void init(const VulkanContext& context, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void recreate(const VulkanContext& context, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void destroy(VkDevice device);

private:
    void createSwapchain(const VulkanContext& context, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void createImageViews(VkDevice device);
    
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
};