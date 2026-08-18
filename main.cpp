#include "Renderer/Renderer.h"
#include "Particles/ParticleSystem.h"
#include "Renderer/ImGuiManager.h"
#include "Renderer/StartingScreen.h"
#include "Config/Config.h"
#include "audio/WasapiCapture.h"
#include "audio/AudioConfig/AudioConfig.h"

#include <SFML/Window.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <optional>
#include <thread>

int main()
{
    try
    {
        Config::load("Config/config.ini");
        std::thread audioThread;
        sf::Window window(
            sf::VideoMode({Config::window.width,
                           Config::window.height}),
            "Particle Simulation",
            sf::Style::Default,
            sf::State::Windowed);

        std::vector<const char *> extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
        };

        VulkanContext vulkanContext;
        vulkanContext.initInstance(extensions);

        Renderer renderer;
        renderer.init(vulkanContext, window);

        ImGuiManager imgui;
        imgui.init(
            vulkanContext,
            window,
            renderer.getSwapchainFormat());

        StartingScreen startingScreen;
        ParticleSystem particles;

        bool running = true;
        bool simulationStarted = false;

        ControlMode controlMode = ControlMode::None;

        AudioConfig audioConfig;
        WasapiCapture wasapiCapture;

        while (running)
        {
            while (const std::optional event = window.pollEvent())
            {
                imgui.processEvent(*event);

                if (event->is<sf::Event::Closed>())
                    running = false;
            }

            imgui.newFrame(window);

            if (!simulationStarted)
            {
                controlMode = startingScreen.render();

                if (controlMode != ControlMode::None)
                {
                    simulationStarted = true;

                    const char *shaderPath =
                        controlMode == ControlMode::Mouse
                            ? "shaders/particle.comp.spv"
                            : "shaders/music.comp.spv";

                    uint32_t pushConstantSize =
                        controlMode == ControlMode::Mouse
                            ? sizeof(ComputePush)
                            : sizeof(MusicPush);

                    particles.init(
                        vulkanContext,
                        Config::particles.count,
                        Config::window.width,
                        Config::window.height,
                        shaderPath,
                        pushConstantSize);

                    renderer.setParticleBuffer(
                        particles.getBuffer(),
                        particles.getCount());

                    renderer.setComputeFinishedSemaphore(
                        particles.getComputeFinishedSemaphore());

                    if (controlMode == ControlMode::Music)
                    {
                        audioConfig.load(
                            "audio/AudioConfig/AudioConfig.ini");

                        if (!wasapiCapture.init())
                        {
                            throw std::runtime_error(
                                "Failed to initialize WASAPI capture.");
                        }

                        audioThread = std::thread(
                            [&wasapiCapture]()
                            {
                                wasapiCapture.run();
                            });
                    }
                }
            }

            if (simulationStarted)
            {
                if (controlMode == ControlMode::Mouse)
                {
                    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                    ComputePush push{};
                    push.mouseX = static_cast<float>(mousePos.x);
                    push.mouseY = static_cast<float>(mousePos.y);
                    particles.update(
                        vulkanContext,
                        &push,
                        sizeof(push));
                }
                else if (controlMode == ControlMode::Music)
                {
                    const MusicPush::Data &push = wasapiCapture.getMusicPush();
                    //MusicPush::printData(push);
                    particles.update(
                        vulkanContext,
                        &push,
                        sizeof(push));
                }
            }

            renderer.render(imgui);
        }

        vkDeviceWaitIdle(vulkanContext.device);

        imgui.destroy();
        particles.destroy(vulkanContext.device);
        renderer.destroy();
        vulkanContext.destroy();
    }
    catch (const std::exception &e)
    {
        std::cerr
            << "FATAL ERROR: "
            << e.what()
            << '\n';

        return 1;
    }

    return 0;
}