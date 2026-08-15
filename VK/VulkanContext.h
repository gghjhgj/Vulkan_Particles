#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>
#include <vector>

#define ENABLE_VALIDATION_LAYERS
class VulkanContext
{
public:
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    
    uint32_t graphicsQueueFamilyIndex = 0;
    uint32_t computeQueueFamilyIndex = 0;

    void initInstance(const std::vector<const char*>& instanceExtensions);
    void initDevice(VkSurfaceKHR surface);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    void destroy();

private:
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    void setupDebugMessenger();
    void createInstance(const std::vector<const char*>& instanceExtensions);
    void pickPhysicalDevice();
    void createLogicalDevice(VkSurfaceKHR surface);
};