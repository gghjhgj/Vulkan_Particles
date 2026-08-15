#pragma once

#include "../VK/VulkanContext.h"

#include <SFML/Window.hpp>

#include <vulkan/vulkan.h>

#include <chrono>

class ImGuiManager
{
public:

    ~ImGuiManager();

    void init(
        VulkanContext& context,
        sf::Window& window,
        VkFormat swapchainFormat
    );

    void processEvent(
        const sf::Event& event
    );

    void newFrame(
        sf::Window& window
    );

    void render(
        VkCommandBuffer commandBuffer
    );

    void destroy();

private:

    VulkanContext* vkContext =
        nullptr;

    bool initialized =
        false;

    std::chrono::steady_clock::time_point
        lastFrameTime;

    float fps =
        0.0f;

    float frameTime =
        0.0f;
};