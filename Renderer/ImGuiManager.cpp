#include "ImGuiManager.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#include <stdexcept>
#include <chrono>
#include <string>

ImGuiManager::~ImGuiManager()
{
    destroy();
}

void ImGuiManager::init(VulkanContext& context, sf::Window& window, VkFormat swapchainFormat)
{
    if (initialized)
        return;

    (void)window;

    vkContext = &context;

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplVulkan_InitInfo initInfo{};

    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = context.instance;
    initInfo.PhysicalDevice = context.physicalDevice;
    initInfo.Device = context.device;
    initInfo.QueueFamily = context.graphicsQueueFamilyIndex;
    initInfo.Queue = context.graphicsQueue;
    initInfo.PipelineCache = VK_NULL_HANDLE;

    initInfo.DescriptorPool = VK_NULL_HANDLE;
    initInfo.DescriptorPoolSize = 1000;

    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;

    initInfo.UseDynamicRendering = true;

    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {};
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;

    initInfo.CheckVkResultFn = [](VkResult result)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error("ImGui Vulkan error: " + std::to_string(result));
    };

    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        ImGui::DestroyContext();
        vkContext = nullptr;

        throw std::runtime_error("Failed to initialize ImGui Vulkan backend.");
    }

    io.Fonts->AddFontDefault();

    initialized = true;

    lastFrameTime = std::chrono::steady_clock::now();

    fps = 0.0f;
    frameTime = 0.0f;
}

void ImGuiManager::processEvent(const sf::Event& event)
{
    if (!initialized)
        return;

    ImGuiIO& io = ImGui::GetIO();

    if (const auto* mouseMoved =
            event.getIf<sf::Event::MouseMoved>())
    {
        io.AddMousePosEvent(
            static_cast<float>(mouseMoved->position.x),
            static_cast<float>(mouseMoved->position.y)
        );
    }

    if (const auto* mouseButtonPressed =
            event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (mouseButtonPressed->button ==
            sf::Mouse::Button::Left)
        {
            io.AddMouseButtonEvent(
                0,
                true
            );
        }
    }

    if (const auto* mouseButtonReleased =
            event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (mouseButtonReleased->button ==
            sf::Mouse::Button::Left)
        {
            io.AddMouseButtonEvent(
                0,
                false
            );
        }
    }

    if (const auto* mouseWheel =
            event.getIf<sf::Event::MouseWheelScrolled>())
    {
        io.AddMouseWheelEvent(
            0.0f,
            mouseWheel->delta
        );
    }
}

void ImGuiManager::newFrame(sf::Window& window)
{
    if (!initialized)
        return;

    ImGuiIO& io = ImGui::GetIO();

    const sf::Vector2u size =
        window.getSize();

    io.DisplaySize =
        ImVec2(
            static_cast<float>(size.x),
            static_cast<float>(size.y)
        );

    const sf::Vector2i mousePos =
        sf::Mouse::getPosition(window);

    io.AddMousePosEvent(
        static_cast<float>(mousePos.x),
        static_cast<float>(mousePos.y)
    );

    const auto now =
        std::chrono::steady_clock::now();

    float dt =
        std::chrono::duration<float>(
            now - lastFrameTime
        ).count();

    lastFrameTime = now;

    if (dt <= 0.0f || dt > 1.0f)
        dt = 1.0f / 60.0f;

    io.DeltaTime = dt;

    frameTime =
        dt * 1000.0f;

    const float currentFPS =
        1.0f / dt;

    constexpr float smoothing = 0.1f;

    if (fps <= 0.0f)
        fps = currentFPS;
    else
        fps +=
            (currentFPS - fps) *
            smoothing;

    ImGui_ImplVulkan_NewFrame();

    ImGui::NewFrame();
}

void ImGuiManager::render(VkCommandBuffer commandBuffer)
{
    if (!initialized)
        return;

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);

    ImGui::Begin(
        "Performance",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav
    );

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame: %.2f ms", frameTime);

    ImGui::End();

    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();

    if (drawData == nullptr)
        return;

    ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
}

void ImGuiManager::destroy()
{
    if (!initialized)
        return;

    if (vkContext && vkContext->device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(vkContext->device);

        ImGui_ImplVulkan_Shutdown();

        ImGui::DestroyContext();
    }

    vkContext = nullptr;
    initialized = false;
    fps = 0.0f;
    frameTime = 0.0f;
}