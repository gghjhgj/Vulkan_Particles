#include "Renderer/Renderer.h"
#include "Simulation/Particles/ParticleSystem.h"
#include "Simulation/Fluids/FluidSystem.h"
#include "Simulation/FluidParticles/FluidParticles.h"
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
            "Simulation System",
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
        FluidSystem fluid;
        FluidParticles fluidParticles;

        bool running = true;
        bool simulationStarted = false;

        ControlMode controlMode = ControlMode::None;

        AudioConfig audioConfig;
        WasapiCapture wasapiCapture;

        sf::Vector2i lastMousePos = sf::Mouse::getPosition(window);
        sf::Clock clock;

        while (running)
        {
            while (const std::optional event = window.pollEvent())
            {
                imgui.processEvent(*event);

                if (event->is<sf::Event::Closed>())
                    running = false;
            }

            float dt = clock.restart().asSeconds();
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);

            imgui.newFrame(window);

            if (!simulationStarted)
            {
                controlMode = startingScreen.render();

                if (controlMode != ControlMode::None)
                {
                    simulationStarted = true;

                    if (controlMode == ControlMode::MouseParticles || controlMode == ControlMode::MusicParticles)
                    {
                        const char *shaderPath =
                            controlMode == ControlMode::MouseParticles
                                ? "shaders/particles/particle.comp.spv"
                                : "shaders/particles/music.comp.spv";

                        uint32_t pushConstantSize =
                            controlMode == ControlMode::MouseParticles
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

                        if (controlMode == ControlMode::MusicParticles)
                        {
                            audioConfig.load("audio/AudioConfig/AudioConfig.ini");

                            if (!wasapiCapture.init())
                            {
                                throw std::runtime_error("Failed to initialize WASAPI capture.");
                            }

                            audioThread = std::thread(
                                [&wasapiCapture]()
                                {
                                    wasapiCapture.run();
                                });
                        }
                    }
                    else if (controlMode == ControlMode::FluidMouse)
                    {
                        fluid.init(
                            vulkanContext,
                            Config::fluid.simWidth,
                            Config::fluid.simHeight,
                            "shaders/fluids/fluid.comp.spv",
                            sizeof(FluidPushConstants));

                        renderer.setFluidBuffer(
                            fluid.getActiveBuffer(),
                            Config::fluid.simWidth,
                            Config::fluid.simHeight);

                        renderer.setComputeFinishedSemaphore(
                            fluid.getComputeFinishedSemaphore());
                    }
                    else if (controlMode == ControlMode::FluidParticles)
                    {
                        fluid.init(
                            vulkanContext,
                            Config::fluid.simWidth,
                            Config::fluid.simHeight,
                            "shaders/fluids/fluid.comp.spv",
                            sizeof(FluidPushConstants));

                        particles.init(
                            vulkanContext,
                            Config::particles.count,
                            Config::window.width,
                            Config::window.height,
                            "shaders/particles/particle.comp.spv",
                            sizeof(ComputePush));

                        fluidParticles.init(
                            vulkanContext,
                            particles,
                            fluid,
                            "shaders/fluidParticles/fluid_particles.comp.spv",
                            sizeof(FluidPushConstants));

                        renderer.setParticleBuffer(
                            particles.getBuffer(),
                            particles.getCount());

                        renderer.setFluidBuffer(
                            fluid.getActiveBuffer(),
                            Config::fluid.simWidth,
                            Config::fluid.simHeight);

                        renderer.setComputeFinishedSemaphore(
                            fluid.getComputeFinishedSemaphore());
                    }
                    else if (controlMode == ControlMode::FluidParticlesMusic)
                    {
                    }
                }
            }

            if (simulationStarted)
            {
                if (controlMode == ControlMode::MouseParticles)
                {
                    ComputePush push{};
                    push.mouseX = static_cast<float>(mousePos.x);
                    push.mouseY = static_cast<float>(mousePos.y);

                    particles.update(
                        vulkanContext,
                        &push,
                        sizeof(push));
                }
                else if (controlMode == ControlMode::MusicParticles)
                {
                    const MusicPush::Data &push = wasapiCapture.getMusicPush();

                    particles.update(
                        vulkanContext,
                        &push,
                        sizeof(push));
                }
                else if (controlMode == ControlMode::FluidMouse)
                {
                    FluidPushConstants push{};
                    push.mouseX = static_cast<float>(mousePos.x);
                    push.mouseY = static_cast<float>(mousePos.y);
                    push.prevMouseX = static_cast<float>(lastMousePos.x);
                    push.prevMouseY = static_cast<float>(lastMousePos.y);
                    push.dt = dt;
                    push.splatRadius = Config::fluid.splatRadius;
                    push.splatForce = Config::fluid.splatForce;
                    push.velocityDissipation = Config::fluid.velocityDissipation;
                    push.densityDissipation = Config::fluid.densityDissipation;
                    push.vorticity = Config::fluid.vorticity;
                    push.windowWidth = Config::window.width;
                    push.windowHeight = Config::window.height;
                    push.simWidth = Config::fluid.simWidth;
                    push.simHeight = Config::fluid.simHeight;
                    push.isMouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) ? 1 : 0;

                    fluid.update(
                        vulkanContext,
                        &push,
                        sizeof(FluidPushConstants));

                    renderer.setFluidBuffer(
                        fluid.getActiveBuffer(),
                        Config::fluid.simWidth,
                        Config::fluid.simHeight);
                }
else if (controlMode == ControlMode::FluidParticles)
                {
                    FluidPushConstants push{};
                    push.mouseX = static_cast<float>(mousePos.x);
                    push.mouseY = static_cast<float>(mousePos.y);
                    push.prevMouseX = static_cast<float>(lastMousePos.x);
                    push.prevMouseY = static_cast<float>(lastMousePos.y);
                    push.dt = dt;
                    push.splatRadius = Config::fluid.splatRadius;
                    push.splatForce = Config::fluid.splatForce;
                    push.velocityDissipation = Config::fluid.velocityDissipation;
                    push.densityDissipation = Config::fluid.densityDissipation;
                    push.vorticity = Config::fluid.vorticity;
                    push.windowWidth = Config::window.width;
                    push.windowHeight = Config::window.height;
                    push.simWidth = Config::fluid.simWidth;
                    push.simHeight = Config::fluid.simHeight;
                    push.isMouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) ? 1 : 0;

                    fluidParticles.update(
                        vulkanContext,
                        particles,
                        fluid,
                        &push,
                        sizeof(FluidPushConstants));

                    fluid.update(
                        vulkanContext,
                        &push,
                        sizeof(FluidPushConstants),
                        fluidParticles.getComputeFinishedSemaphore());

                    renderer.setFluidBuffer(
                        fluid.getActiveBuffer(),
                        Config::fluid.simWidth,
                        Config::fluid.simHeight);
                }
                else if (controlMode == ControlMode::FluidParticlesMusic)
                {
                }
            }

            lastMousePos = mousePos;

            renderer.render(imgui);
        }

        vkDeviceWaitIdle(vulkanContext.device);

        imgui.destroy();

        if (controlMode == ControlMode::FluidMouse)
        {
            fluid.destroy(vulkanContext.device);
        }
        else if (controlMode == ControlMode::FluidParticles)
        {
            fluid.destroy(vulkanContext.device);
            particles.destroy(vulkanContext.device);
            fluidParticles.destroy(vulkanContext.device);
        }
        else if (controlMode == ControlMode::FluidParticlesMusic)
        {
            fluid.destroy(vulkanContext.device);
            particles.destroy(vulkanContext.device);
            fluidParticles.destroy(vulkanContext.device);
        }
        else if (controlMode == ControlMode::MouseParticles || controlMode == ControlMode::MusicParticles)
        {
            particles.destroy(vulkanContext.device);
        }

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