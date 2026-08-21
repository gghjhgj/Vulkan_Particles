#include "Renderer/Renderer.h"
#include "Simulation/Particles/ParticleSystem.h"
#include "Simulation/Fluids/FluidSystem.h"
#include "Simulation/FluidParticles/FluidParticles.h"
#include "Simulation/FluidParticles/FluidParticlesMusicPushConstants.h"
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
            sf::VideoMode({Config::window.width, Config::window.height}),
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
        imgui.init(vulkanContext, window, renderer.getSwapchainFormat());

        StartingScreen startingScreen;
        ParticleSystem particles;
        FluidSystem fluid;
        FluidParticles fluidParticles;

        bool running = true;
        bool simulationStarted = false;

        ControlMode controlMode = ControlMode::None;

        AudioConfig audioConfig;
        WasapiCapture wasapiCapture;
        FluidParticlesMusicPushConstants fluidParticlesMusicPush;

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
                    lastMousePos = sf::Mouse::getPosition(window);

                    if (controlMode == ControlMode::MouseParticles || controlMode == ControlMode::MusicParticles)
                    {
                        const char *shaderPath = controlMode == ControlMode::MouseParticles
                            ? "shaders/particles/particle.comp.spv"
                            : "shaders/particles/music.comp.spv";

                        uint32_t pushConstantSize = controlMode == ControlMode::MouseParticles
                            ? sizeof(ComputePush)
                            : sizeof(MusicPush::Data);

                        particles.init(vulkanContext, Config::particles.count, Config::window.width, Config::window.height, shaderPath, pushConstantSize);
                        
                        renderer.setParticleBuffer(particles.getBuffer(), particles.getCount());
                        renderer.setComputeFinishedSemaphore(particles.getComputeFinishedSemaphore());

                        if (controlMode == ControlMode::MusicParticles)
                        {
                            audioConfig.load("audio/AudioConfig/AudioConfig.ini");
                            if (!wasapiCapture.init())
                                throw std::runtime_error("Failed to initialize WASAPI capture.");

                            audioThread = std::thread([&wasapiCapture]() { wasapiCapture.run(); });
                        }
                    }
                    else if (controlMode == ControlMode::FluidMouse)
                    {
                        fluid.init(vulkanContext, Config::fluid.simWidth, Config::fluid.simHeight, "shaders/fluids/fluid.comp.spv", sizeof(FluidPushConstants));
                        
                        renderer.setFluidTexture(fluid.getActiveColorTexture(), Config::fluid.simWidth, Config::fluid.simHeight);
                        renderer.setComputeFinishedSemaphore(fluid.getComputeFinishedSemaphore());
                    }
                    else if (controlMode == ControlMode::FluidParticles || controlMode == ControlMode::FluidParticlesMusic)
                    {
                        fluid.init(vulkanContext, Config::fluid.simWidth, Config::fluid.simHeight, "shaders/fluids/fluid.comp.spv", sizeof(FluidPushConstants));
                        particles.init(vulkanContext, Config::particles.count, Config::window.width, Config::window.height,
                            controlMode == ControlMode::FluidParticles ? "shaders/particles/particle.comp.spv" : "shaders/particles/music.comp.spv",
                            controlMode == ControlMode::FluidParticles ? sizeof(ComputePush) : sizeof(MusicPush::Data));

                        const char *shaderPath = controlMode == ControlMode::FluidParticles
                            ? "shaders/fluidParticles/fluid_particles.comp.spv"
                            : "shaders/fluidParticles/fluid_particles_music.comp.spv";

                        uint32_t pushConstantSize = controlMode == ControlMode::FluidParticles
                            ? sizeof(FluidPushConstants)
                            : sizeof(FluidParticlesMusicPushConstants::Data);

                        fluidParticles.init(vulkanContext, particles, fluid, shaderPath, pushConstantSize);
                        
                        renderer.setParticleBuffer(particles.getBuffer(), particles.getCount());
                        renderer.setFluidTexture(fluid.getActiveColorTexture(), Config::fluid.simWidth, Config::fluid.simHeight);
                        renderer.setComputeFinishedSemaphore(fluid.getComputeFinishedSemaphore());

                        if (controlMode == ControlMode::FluidParticlesMusic)
                        {
                            audioConfig.load("audio/AudioConfig/AudioConfig.ini");
                            if (!wasapiCapture.init())
                                throw std::runtime_error("Failed to initialize WASAPI capture.");

                            audioThread = std::thread([&wasapiCapture]() { wasapiCapture.run(); });
                        }
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

                    particles.update(vulkanContext, &push, sizeof(push));
                }
                else if (controlMode == ControlMode::MusicParticles)
                {
                    const MusicPush::Data &push = wasapiCapture.getMusicPush();
                    particles.update(vulkanContext, &push, sizeof(push));
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

                    fluid.update(vulkanContext, &push, sizeof(FluidPushConstants));
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

                    fluidParticles.update(vulkanContext, particles, fluid, &push, sizeof(FluidPushConstants));
                    fluid.update(vulkanContext, &push, sizeof(FluidPushConstants), fluidParticles.getComputeFinishedSemaphore());
                }
                else if (controlMode == ControlMode::FluidParticlesMusic)
                {
                    const MusicPush::Data &musicData = wasapiCapture.getMusicPush();

                    FluidPushConstants fluidPush{};
                    fluidPush.dt = dt;
                    fluidPush.splatRadius = Config::fluid.splatRadius;
                    fluidPush.splatForce = Config::fluid.splatForce;
                    fluidPush.velocityDissipation = Config::fluid.velocityDissipation;
                    fluidPush.densityDissipation = Config::fluid.densityDissipation;
                    fluidPush.vorticity = Config::fluid.vorticity;
                    fluidPush.windowWidth = Config::window.width;
                    fluidPush.windowHeight = Config::window.height;
                    fluidPush.simWidth = Config::fluid.simWidth;
                    fluidPush.simHeight = Config::fluid.simHeight;

                    fluidParticlesMusicPush.update(musicData, dt, Config::fluid.splatRadius, Config::fluid.splatForce,
                        Config::fluid.velocityDissipation, Config::fluid.densityDissipation, Config::fluid.vorticity,
                        Config::fluid.simWidth, Config::fluid.simHeight, Config::window.width, Config::window.height);

                    const auto &fpData = fluidParticlesMusicPush.get();

                    fluidParticles.update(vulkanContext, particles, fluid, &fpData, sizeof(FluidParticlesMusicPushConstants::Data));
                    fluid.update(vulkanContext, &fluidPush, sizeof(FluidPushConstants), fluidParticles.getComputeFinishedSemaphore());
                }
            }

            lastMousePos = mousePos;
            renderer.render(imgui);
        }

        wasapiCapture.stop();
        if (audioThread.joinable())
        {
            audioThread.join();
        }

        vkDeviceWaitIdle(vulkanContext.device);

        imgui.destroy();

        if (controlMode == ControlMode::FluidMouse)
        {
            fluid.destroy(vulkanContext.device);
        }
        else if (controlMode == ControlMode::FluidParticles || controlMode == ControlMode::FluidParticlesMusic)
        {
            fluidParticles.destroy(vulkanContext.device);
            particles.destroy(vulkanContext.device);
            fluid.destroy(vulkanContext.device);
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
        std::cerr << "FATAL ERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}