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
#include <algorithm>

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
        float warmupTimer = 0.0f;

        bool needsGpuFlush = false;
        sf::Clock renderTimer;

        ControlMode controlMode = ControlMode::None;

        AudioConfig audioConfig;
        WasapiCapture wasapiCapture;
        FluidParticlesMusicPushConstants fluidParticlesMusicPush;

        sf::Vector2i lastMousePos = sf::Mouse::getPosition(window);
        bool wasMouseDown = false;
        sf::Clock clock;

        while (running)
        {
            while (const std::optional event = window.pollEvent())
            {
                imgui.processEvent(*event);

                if (event->is<sf::Event::Closed>())
                    running = false;
            }

            if (simulationStarted && (warmupTimer < 2.0f || needsGpuFlush))
            {
                vkDeviceWaitIdle(vulkanContext.device);
                needsGpuFlush = false;
            }

            float rawDt = clock.restart().asSeconds();
            float dt = std::min(rawDt, 0.033f);

            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            bool isMouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

            imgui.newFrame(window);

            if (!simulationStarted)
            {
                controlMode = startingScreen.render();

                if (controlMode != ControlMode::None)
                {
                    simulationStarted = true;
                    warmupTimer = 0.0f;
                    needsGpuFlush = true;

                    clock.restart();

                    lastMousePos = sf::Mouse::getPosition(window);
                    wasMouseDown = isMouseDown;

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
                        
                        renderer.setFluidTexture(fluid.getActiveColorTexture(), fluid.getSimWidth(), fluid.getSimHeight());
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
                        renderer.setFluidTexture(fluid.getActiveColorTexture(), fluid.getSimWidth(), fluid.getSimHeight());
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
                warmupTimer += dt;
                float audioRampUp = std::clamp(warmupTimer / 2.0f, 0.0f, 1.0f);

                if (controlMode == ControlMode::MouseParticles)
                {
                    ComputePush push{};
                    push.mouseX = static_cast<float>(mousePos.x);
                    push.mouseY = static_cast<float>(mousePos.y);

                    particles.update(vulkanContext, &push, sizeof(push));
                }
                else if (controlMode == ControlMode::MusicParticles)
                {
                    MusicPush::Data push = wasapiCapture.getMusicPush();
                    
                    push.bass *= audioRampUp;
                    push.mid *= audioRampUp;
                    push.treble *= audioRampUp;
                    push.rms *= audioRampUp;

                    particles.update(vulkanContext, &push, sizeof(push));
                }
                else if (controlMode == ControlMode::FluidMouse)
                {
                    float curX = static_cast<float>(mousePos.x);
                    float curY = static_cast<float>(mousePos.y);
                    float prevX = wasMouseDown ? static_cast<float>(lastMousePos.x) : curX;
                    float prevY = wasMouseDown ? static_cast<float>(lastMousePos.y) : curY;

                    FluidPushConstants push{};
                    push.mouseX = curX;
                    push.mouseY = curY;
                    push.prevMouseX = prevX;
                    push.prevMouseY = prevY;
                    push.dt = dt;
                    push.splatRadius = Config::fluid.splatRadius;
                    push.splatForce = Config::fluid.splatForce;
                    push.velocityDissipation = Config::fluid.velocityDissipation;
                    push.densityDissipation = Config::fluid.densityDissipation;
                    push.vorticity = Config::fluid.vorticity;

                    push.renderWidth  = fluid.getSimWidth();
                    push.renderHeight = fluid.getSimHeight();
                    push.simWidth     = fluid.getSimWidth();
                    push.simHeight    = fluid.getSimHeight();
                    push.pressWidth   = fluid.getPressWidth();
                    push.pressHeight  = fluid.getPressHeight();
                    push.windowWidth  = Config::window.width;
                    push.windowHeight = Config::window.height;
                    
                    push.isMouseDown = isMouseDown ? 1 : 0;
                    push.offsetFromLeft = Config::fluid.offsetFromLeft;
                    push.offsetFromRight = Config::fluid.offsetFromRight;
                    push.omega = Config::fluid.omega;
                    push.pressureSteps = Config::fluid.pressureSteps;

                    fluid.update(vulkanContext, &push, sizeof(FluidPushConstants));
                    renderer.setFluidTexture(fluid.getActiveColorTexture(), fluid.getSimWidth(), fluid.getSimHeight());
                }
                else if (controlMode == ControlMode::FluidParticles)
                {
                    float curX = static_cast<float>(mousePos.x);
                    float curY = static_cast<float>(mousePos.y);
                    float prevX = wasMouseDown ? static_cast<float>(lastMousePos.x) : curX;
                    float prevY = wasMouseDown ? static_cast<float>(lastMousePos.y) : curY;

                    FluidPushConstants push{};
                    push.mouseX = curX;
                    push.mouseY = curY;
                    push.prevMouseX = prevX;
                    push.prevMouseY = prevY;
                    push.dt = dt;
                    push.splatRadius = Config::fluid.splatRadius;
                    push.splatForce = Config::fluid.splatForce;
                    push.velocityDissipation = Config::fluid.velocityDissipation;
                    push.densityDissipation = Config::fluid.densityDissipation;
                    push.vorticity = Config::fluid.vorticity;

                    push.renderWidth  = fluid.getSimWidth();
                    push.renderHeight = fluid.getSimHeight();
                    push.simWidth     = fluid.getSimWidth();
                    push.simHeight    = fluid.getSimHeight();
                    push.pressWidth   = fluid.getPressWidth();
                    push.pressHeight  = fluid.getPressHeight();
                    push.windowWidth  = Config::window.width;
                    push.windowHeight = Config::window.height;

                    push.isMouseDown = isMouseDown ? 1 : 0;
                    push.offsetFromLeft = Config::fluid.offsetFromLeft;
                    push.offsetFromRight = Config::fluid.offsetFromRight;
                    push.omega = Config::fluid.omega;
                    push.pressureSteps = Config::fluid.pressureSteps;

                    fluidParticles.update(vulkanContext, particles, fluid, &push, sizeof(FluidPushConstants));
                    fluid.update(vulkanContext, &push, sizeof(FluidPushConstants), fluidParticles.getComputeFinishedSemaphore());
                    renderer.setFluidTexture(fluid.getActiveColorTexture(), fluid.getSimWidth(), fluid.getSimHeight());
                }
                else if (controlMode == ControlMode::FluidParticlesMusic)
                {
                    const MusicPush::Data &musicData = wasapiCapture.getMusicPush();

                    FluidPushConstants fluidPush{};
                    fluidPush.mouseX = 0.0f;
                    fluidPush.mouseY = 0.0f;
                    fluidPush.prevMouseX = 0.0f;
                    fluidPush.prevMouseY = 0.0f;
                    fluidPush.dt = dt;
                    fluidPush.splatRadius = Config::fluid.splatRadius;
                    fluidPush.splatForce = Config::fluid.splatForce * audioRampUp;
                    fluidPush.velocityDissipation = Config::fluid.velocityDissipation;
                    fluidPush.densityDissipation = Config::fluid.densityDissipation;
                    fluidPush.vorticity = Config::fluid.vorticity;

                    fluidPush.renderWidth  = fluid.getSimWidth();
                    fluidPush.renderHeight = fluid.getSimHeight();
                    fluidPush.simWidth     = fluid.getSimWidth();
                    fluidPush.simHeight    = fluid.getSimHeight();
                    fluidPush.pressWidth   = fluid.getPressWidth();
                    fluidPush.pressHeight  = fluid.getPressHeight();
                    fluidPush.windowWidth  = Config::window.width;
                    fluidPush.windowHeight = Config::window.height;

                    fluidPush.isMouseDown = 0;
                    fluidPush.offsetFromLeft = Config::fluid.offsetFromLeft;
                    fluidPush.offsetFromRight = Config::fluid.offsetFromRight;
                    fluidPush.offsetFromUp = Config::fluid.offsetFromUp;
                    fluidPush.offsetFromDown = Config::fluid.offsetFromDown;
                    fluidPush.omega = Config::fluid.omega;
                    fluidPush.pressureSteps = Config::fluid.pressureSteps;

                    fluidParticlesMusicPush.update(musicData, dt, Config::fluid.splatRadius, Config::fluid.splatForce * audioRampUp,
                        Config::fluid.velocityDissipation, Config::fluid.densityDissipation, Config::fluid.vorticity,
                        fluid.getSimWidth(), fluid.getSimHeight(), Config::window.width, Config::window.height);

                    const auto &fpData = fluidParticlesMusicPush.get();

                    fluidParticles.update(vulkanContext, particles, fluid, &fpData, sizeof(FluidParticlesMusicPushConstants::Data));
                    fluid.update(vulkanContext, &fluidPush, sizeof(FluidPushConstants), fluidParticles.getComputeFinishedSemaphore());
                    renderer.setFluidTexture(fluid.getActiveColorTexture(), fluid.getSimWidth(), fluid.getSimHeight());
                }
            }

            lastMousePos = mousePos;
            wasMouseDown = isMouseDown;

            renderTimer.restart();
            renderer.render(imgui);
            float renderDuration = renderTimer.getElapsedTime().asSeconds();

            if (renderDuration > 0.040f)
            {
                needsGpuFlush = true;
            }
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