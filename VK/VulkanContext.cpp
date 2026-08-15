#include "VulkanContext.h"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>

#ifdef ENABLE_VALIDATION_LAYERS
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    std::cerr << "[Validation Layer]: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) func(instance, debugMessenger, pAllocator);
}
#endif

void VulkanContext::initInstance(const std::vector<const char*>& instanceExtensions) {
    createInstance(instanceExtensions);
    pickPhysicalDevice();
}

void VulkanContext::initDevice(VkSurfaceKHR surface) {
    createLogicalDevice(surface);
}

void VulkanContext::destroy() {
#ifdef ENABLE_VALIDATION_LAYERS
    if (instance != VK_NULL_HANDLE && debugMessenger != VK_NULL_HANDLE) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
#endif
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

void VulkanContext::createInstance(const std::vector<const char*>& instanceExtensions) 
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Compute & Graphics World";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "AAA Custom Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = instanceExtensions;
#ifdef ENABLE_VALIDATION_LAYERS
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef ENABLE_VALIDATION_LAYERS
    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
#else
    createInfo.enabledLayerCount = 0;
#endif

    if(vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Vulkan instance create crashed");
    }

#ifdef ENABLE_VALIDATION_LAYERS
    setupDebugMessenger();
#endif

    std::cout << "VK instance created with SFML window extensions." << std::endl;
}

void VulkanContext::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
        throw std::runtime_error("failed to set up debug messenger!");
}

void VulkanContext::pickPhysicalDevice() 
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if(deviceCount == 0) throw std::runtime_error("No graphics card");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for(const auto& dev : devices)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(dev, &deviceProperties);
        if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            physicalDevice = dev;
            std::cout << "Integrated graphics card selected: " << deviceProperties.deviceName << std::endl;
            break;
        }
    }
    if(physicalDevice == VK_NULL_HANDLE) {
        physicalDevice = devices[0];
        std::cout << "No integrated card, selected fallback." << std::endl;
    }
}

void VulkanContext::createLogicalDevice(VkSurfaceKHR surface)
{
    uint32_t queueFamilyCount = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        nullptr
    );

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        queueFamilies.data()
    );

    bool foundGraphics = false;
    bool foundCompute = false;

    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        VkBool32 presentSupport = false;

        vkGetPhysicalDeviceSurfaceSupportKHR(
            physicalDevice,
            i,
            surface,
            &presentSupport
        );

        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            presentSupport)
        {
            graphicsQueueFamilyIndex = i;
            foundGraphics = true;
        }

        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            computeQueueFamilyIndex = i;
            foundCompute = true;
        }

        if (foundGraphics && foundCompute)
            break;
    }

    if (!foundGraphics || !foundCompute)
        throw std::runtime_error("Could not find queues.");

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    std::vector<uint32_t> uniqueQueueFamilies =
    {
        graphicsQueueFamilyIndex
    };

    if (graphicsQueueFamilyIndex != computeQueueFamilyIndex)
        uniqueQueueFamilies.push_back(computeQueueFamilyIndex);

    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};

        queueCreateInfo.sType =
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        queueCreateInfo.queueFamilyIndex =
            queueFamily;

        queueCreateInfo.queueCount = 1;

        queueCreateInfo.pQueuePriorities =
            &queuePriority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    const std::vector<const char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures deviceFeatures{};

    deviceFeatures.largePoints = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vulkan13Features{};

    vulkan13Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    vulkan13Features.dynamicRendering =
        VK_TRUE;

    VkDeviceCreateInfo createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.pNext =
        &vulkan13Features;

    createInfo.pEnabledFeatures =
        &deviceFeatures;

    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(
            queueCreateInfos.size()
        );

    createInfo.pQueueCreateInfos =
        queueCreateInfos.data();

    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(
            deviceExtensions.size()
        );

    createInfo.ppEnabledExtensionNames =
        deviceExtensions.data();

    if (vkCreateDevice(
            physicalDevice,
            &createInfo,
            nullptr,
            &device
        ) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "couldn't create logicDevice"
        );
    }

    vkGetDeviceQueue(
        device,
        graphicsQueueFamilyIndex,
        0,
        &graphicsQueue
    );

    vkGetDeviceQueue(
        device,
        computeQueueFamilyIndex,
        0,
        &computeQueue
    );
}

uint32_t VulkanContext::findMemoryType(
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;

    vkGetPhysicalDeviceMemoryProperties(
        physicalDevice,
        &memProperties);


    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if((typeFilter & (1 << i)) &&
           (memProperties.memoryTypes[i].propertyFlags & properties)
              == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("Nie znaleziono odpowiedniego typu pamieci");
}