#pragma once

#include "VK/VulkanContext.h"
#include "VK/VulkanSwapchain.h"
#include "VK/VulkanFrameData.h"
#include "VK/VulkanImageUtils.h"
#include "VK/VulkanGraphicsPipeline.h"
#include "VK/VulkanBuffer.h"
#include "../Config/Config.h"
#include <SFML/Window.hpp>

#include <vector>
#include <cstdint>

class ImGuiManager;

struct ParticleRenderPushConstant
{
    float width;
    float height;

    float size;
    float trail_length;
    float trail_width;
};

class Renderer
{
public:
    VkFormat getSwapchainFormat() const;

    std::vector<VkSemaphore> renderFinishedSemaphores;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    Renderer() = default;
    ~Renderer();

    void init(
        VulkanContext& context,
        sf::Window& window
    );

    void setParticleBuffer(
        const VulkanBuffer& buffer,
        uint32_t particleCount
    );

    void render(ImGuiManager& imgui);

    void destroy();

    void setComputeFinishedSemaphore(
        VkSemaphore semaphore
    );

private:
    void createSurface(sf::Window& window);
    void createParticlePipeline();
    void createParticleDescriptors();

private:
    VulkanContext* vkContext = nullptr;

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VulkanSwapchain swapchain;

    std::vector<VulkanFrameData> frames;

    std::vector<VkImageLayout> swapchainLayouts;

    VulkanGraphicsPipeline particleGraphicsPipeline;

    VkDescriptorSetLayout particleDescriptorSetLayout = VK_NULL_HANDLE;

    VkDescriptorPool particleDescriptorPool = VK_NULL_HANDLE;

    VkDescriptorSet particleDescriptorSet = VK_NULL_HANDLE;

    const VulkanBuffer* particleBuffer = nullptr;

    uint32_t particleCount = 0;

    uint32_t currentFrame = 0;

    bool initialized = false;
    bool particlesConfigured = false;

    VkSemaphore computeFinishedSemaphore = VK_NULL_HANDLE;
};