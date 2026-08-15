#include "Renderer/Renderer.h"
#include "Particles/ParticleSystem.h"
#include "Renderer/ImGuiManager.h"
#include "Config/Config.h"
#include <SFML/Window.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <optional>

int main()
{
    try
    {
        Config::load("Config/config.ini");
        sf::Window window(
            sf::VideoMode({Config::window.width, Config::window.height}),
            "Particle Simulation",
            sf::Style::Default,
            sf::State::Windowed
        );

        std::vector<const char*> extensions =
        {
            VK_KHR_SURFACE_EXTENSION_NAME,

#ifdef _WIN32
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
        };

        VulkanContext vulkanContext;

        vulkanContext.initInstance(
            extensions
        );

        Renderer renderer;

        renderer.init(
            vulkanContext,
            window
        );

        ParticleSystem particles;

        particles.init(
            vulkanContext,
            Config::particles.count,
            1280,
            720
        );

        renderer.setParticleBuffer(
            particles.getBuffer(),
            particles.getCount()
        );

        ImGuiManager imgui;

        imgui.init(
            vulkanContext,
            window,
            renderer.getSwapchainFormat()
        );

        bool running = true;

        while (running)
        {
            while (const std::optional event = window.pollEvent())
            {
                imgui.processEvent(*event);

                if (event->is<sf::Event::Closed>())
                {
                    running = false;
                }

                if (const auto* resized =
                        event->getIf<sf::Event::Resized>())
                {
                    (void)resized;
                }
            }

            imgui.newFrame(window);

            sf::Vector2i mousePos =
                sf::Mouse::getPosition(window);

            float mouseX =
                static_cast<float>(mousePos.x);

            float mouseY =
                static_cast<float>(mousePos.y);

            particles.update(
                vulkanContext,
                mouseX,
                mouseY
            );

            renderer.render(imgui);
        }

        vkDeviceWaitIdle(
            vulkanContext.device
        );

        imgui.destroy();

        particles.destroy(
            vulkanContext.device
        );

        renderer.destroy();

        vulkanContext.destroy();
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "FATAL ERROR: "
            << e.what()
            << '\n';

        return 1;
    }

    return 0;
}