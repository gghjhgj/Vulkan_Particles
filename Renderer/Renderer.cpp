#include "Renderer.h"
#include "ImGuiManager.h"

#include <stdexcept>
#include <iostream>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan_win32.h>

#endif

Renderer::~Renderer()
{
    destroy();
}

VkFormat Renderer::getSwapchainFormat() const
{
    return swapchain.imageFormat;
}

void Renderer::setComputeFinishedSemaphore(
    VkSemaphore semaphore)
{
    computeFinishedSemaphore = semaphore;
}

void Renderer::init(
    VulkanContext &context,
    sf::Window &window)
{
    if (initialized)
        return;

    vkContext = &context;

    createSurface(window);

    vkContext->initDevice(surface);

    sf::Vector2u windowSize = window.getSize();

    if (windowSize.x == 0 || windowSize.y == 0)
    {
        throw std::runtime_error("Window has zero size.");
    }

    swapchain.init(
        *vkContext,
        surface,
        windowSize.x,
        windowSize.y);

    swapchainLayouts.resize(
        swapchain.images.size(),
        VK_IMAGE_LAYOUT_UNDEFINED);

    frames.resize(MAX_FRAMES_IN_FLIGHT);

    for (auto &frame : frames)
    {
        frame.init(*vkContext);
    }

    renderFinishedSemaphores.resize(
        swapchain.images.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (auto &semaphore : renderFinishedSemaphores)
    {
        if (vkCreateSemaphore(
                vkContext->device,
                &semaphoreInfo,
                nullptr,
                &semaphore) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render finished semaphore.");
        }
    }

    createParticlePipeline();
    createParticleDescriptors();

    createFluidPipeline();
    createFluidDescriptors();

    initialized = true;

    std::cout
        << "Renderer initialized.\n"
        << "Swapchain: "
        << swapchain.extent.width
        << " x "
        << swapchain.extent.height
        << '\n';
}

void Renderer::createSurface(sf::Window &window)
{
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hwnd = reinterpret_cast<HWND>(window.getNativeHandle());
    surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);

    VkResult result = vkCreateWin32SurfaceKHR(
        vkContext->instance,
        &surfaceCreateInfo,
        nullptr,
        &surface);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan Win32 surface.");
    }
#else
    throw std::runtime_error("This renderer currently supports Windows only.");
#endif
}

void Renderer::createParticlePipeline()
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(
            vkContext->device,
            &layoutInfo,
            nullptr,
            &particleDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create particle descriptor set layout.");
    }

    std::vector<VkDescriptorSetLayout> layouts = { particleDescriptorSetLayout };

    particleGraphicsPipeline.init(
        *vkContext,
        "shaders/particles/particle_vert.spv",
        "shaders/particles/particle_frag.spv",
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        sizeof(ParticleRenderPushConstant),
        swapchain.imageFormat,
        layouts);
}

void Renderer::createParticleDescriptors()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(
            vkContext->device,
            &poolInfo,
            nullptr,
            &particleDescriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create particle descriptor pool.");
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, particleDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = particleDescriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    particleDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateDescriptorSets(
            vkContext->device,
            &allocInfo,
            particleDescriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate particle descriptor sets.");
    }
}

void Renderer::setParticleBuffer(
    const VulkanBuffer &buffer,
    uint32_t count)
{
    particleBuffer = &buffer;
    particleCount = count;
    particlesConfigured = true;
}

void Renderer::createFluidPipeline()
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(
            vkContext->device,
            &layoutInfo,
            nullptr,
            &fluidDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create fluid descriptor set layout.");
    }

    std::vector<VkDescriptorSetLayout> layouts = { fluidDescriptorSetLayout };

    fluidGraphicsPipeline.init(
        *vkContext,
        "shaders/fluids/fluid_vert.spv",
        "shaders/fluids/fluid_frag.spv",
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        sizeof(FluidRenderPushConstant),
        swapchain.imageFormat,
        layouts
    );
}

void Renderer::createFluidDescriptors()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(
            vkContext->device,
            &poolInfo,
            nullptr,
            &fluidDescriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create fluid descriptor pool.");
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, fluidDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = fluidDescriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    fluidDescriptorSet.resize(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateDescriptorSets(
            vkContext->device,
            &allocInfo,
            fluidDescriptorSet.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate fluid descriptor set.");
    }
}

void Renderer::setFluidBuffer(
    const VulkanBuffer &buffer,
    uint32_t simWidth,
    uint32_t simHeight)
{
    fluidBuffer = &buffer;
    fluidSimWidth = simWidth;
    fluidSimHeight = simHeight;
    fluidConfigured = true;
}

void Renderer::render(ImGuiManager &imgui)
{
    if (!initialized)
        return;

    VulkanFrameData &frame = frames[currentFrame];

    VkResult result = vkWaitForFences(
        vkContext->device,
        1,
        &frame.inFlightFence,
        VK_TRUE,
        UINT64_MAX);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to wait for frame fence.");
    }

    if (fluidConfigured && fluidBuffer != nullptr)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = fluidBuffer->handle;
        bufferInfo.offset = 0;
        bufferInfo.range = fluidBuffer->size;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = fluidDescriptorSet[currentFrame];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(
            vkContext->device,
            1,
            &write,
            0,
            nullptr);
    }

    if (particlesConfigured && particleBuffer != nullptr)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = particleBuffer->handle;
        bufferInfo.offset = 0;
        bufferInfo.range = particleBuffer->size;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = particleDescriptorSets[currentFrame];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(
            vkContext->device,
            1,
            &write,
            0,
            nullptr);
    }

    uint32_t imageIndex = 0;

    result = vkAcquireNextImageKHR(
        vkContext->device,
        swapchain.handle,
        UINT64_MAX,
        frame.imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
        return;

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    VkSemaphore renderFinishedSemaphore = renderFinishedSemaphores[imageIndex];

    vkResetFences(vkContext->device, 1, &frame.inFlightFence);

    VkCommandBuffer commandBuffer = frame.commandBuffer;

    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin command buffer.");
    }

    if (computeFinishedSemaphore != VK_NULL_HANDLE)
    {
        VkBuffer syncBufferHandle = VK_NULL_HANDLE;
        VkDeviceSize syncBufferSize = 0;

        if (particleBuffer != nullptr)
        {
            syncBufferHandle = particleBuffer->handle;
            syncBufferSize = particleBuffer->size;
        }
        else if (fluidBuffer != nullptr)
        {
            syncBufferHandle = fluidBuffer->handle;
            syncBufferSize = fluidBuffer->size;
        }

        if (syncBufferHandle != VK_NULL_HANDLE)
        {
            VkBufferMemoryBarrier bufferBarrier{};
            bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferBarrier.buffer = syncBufferHandle;
            bufferBarrier.offset = 0;
            bufferBarrier.size = syncBufferSize;

            if (vkContext->computeQueueFamilyIndex != vkContext->graphicsQueueFamilyIndex)
            {
                bufferBarrier.srcAccessMask = 0;
                bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                bufferBarrier.srcQueueFamilyIndex = vkContext->computeQueueFamilyIndex;
                bufferBarrier.dstQueueFamilyIndex = vkContext->graphicsQueueFamilyIndex;

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0,
                    nullptr,
                    1,
                    &bufferBarrier,
                    0,
                    nullptr);
            }
            else
            {
                bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0,
                    nullptr,
                    1,
                    &bufferBarrier,
                    0,
                    nullptr);
            }
        }
    }

    VulkanImageUtils::transitionImageLayout(
        commandBuffer,
        swapchain.images[imageIndex],
        swapchainLayouts[imageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    swapchainLayouts[imageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchain.imageViews[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = swapchain.extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
        
    if (fluidConfigured && fluidBuffer != nullptr)
    {
        vkCmdBindPipeline(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            fluidGraphicsPipeline.handle);

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            fluidGraphicsPipeline.layout,
            0,
            1,
            &fluidDescriptorSet[currentFrame],
            0,
            nullptr);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain.extent.width);
        viewport.height = static_cast<float>(swapchain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        FluidRenderPushConstant push{};
        push.screenWidth = static_cast<float>(swapchain.extent.width);
        push.screenHeight = static_cast<float>(swapchain.extent.height);
        push.simWidth = fluidSimWidth;
        push.simHeight = fluidSimHeight;

        vkCmdPushConstants(
            commandBuffer,
            fluidGraphicsPipeline.layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(FluidRenderPushConstant),
            &push);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }

    if (particlesConfigured && particleBuffer != nullptr)
    {
        vkCmdBindPipeline(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            particleGraphicsPipeline.handle);

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            particleGraphicsPipeline.layout,
            0,
            1,
            &particleDescriptorSets[currentFrame],
            0,
            nullptr);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain.extent.width);
        viewport.height = static_cast<float>(swapchain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        ParticleRenderPushConstant push{};
        push.width = static_cast<float>(swapchain.extent.width);
        push.height = static_cast<float>(swapchain.extent.height);
        push.size = Config::particles.size;
        push.trail_length = Config::particles.trail_length;
        push.trail_width = Config::particles.trail_width;

        vkCmdPushConstants(
            commandBuffer,
            particleGraphicsPipeline.layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(ParticleRenderPushConstant),
            &push);

        vkCmdDraw(
            commandBuffer,
            particleCount * 4,
            1,
            0,
            0);
    }

    imgui.render(commandBuffer);

    vkCmdEndRendering(commandBuffer);

    VulkanImageUtils::transitionImageLayout(
        commandBuffer,
        swapchain.images[imageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    swapchainLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to end command buffer.");
    }

    VkSemaphore waitSemaphores[2]{};
    VkPipelineStageFlags waitStages[2]{};
    uint32_t waitSemaphoreCount = 0;

    if (computeFinishedSemaphore != VK_NULL_HANDLE)
    {
        waitSemaphores[waitSemaphoreCount] = computeFinishedSemaphore;
        waitStages[waitSemaphoreCount] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        ++waitSemaphoreCount;
    }

    waitSemaphores[waitSemaphoreCount] = frame.imageAvailableSemaphore;
    waitStages[waitSemaphoreCount] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    ++waitSemaphoreCount;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = waitSemaphoreCount;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

    result = vkQueueSubmit(
        vkContext->graphicsQueue,
        1,
        &submitInfo,
        frame.inFlightFence);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit graphics command buffer.");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain.handle;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(vkContext->graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        return;
    }

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::destroy()
{
    if (!vkContext || vkContext->device == VK_NULL_HANDLE)
        return;

    vkDeviceWaitIdle(vkContext->device);

    particleGraphicsPipeline.destroy(vkContext->device);
    fluidGraphicsPipeline.destroy(vkContext->device);

    if (particleDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(vkContext->device, particleDescriptorPool, nullptr);
        particleDescriptorPool = VK_NULL_HANDLE;
    }

    if (fluidDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(vkContext->device, fluidDescriptorPool, nullptr);
        fluidDescriptorPool = VK_NULL_HANDLE;
    }

    if (particleDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vkContext->device, particleDescriptorSetLayout, nullptr);
        particleDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (fluidDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vkContext->device, fluidDescriptorSetLayout, nullptr);
        fluidDescriptorSetLayout = VK_NULL_HANDLE;
    }

    for (auto &frame : frames)
    {
        frame.destroy(vkContext->device);
    }
    frames.clear();

    for (VkSemaphore semaphore : renderFinishedSemaphores)
    {
        if (semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(vkContext->device, semaphore, nullptr);
        }
    }
    renderFinishedSemaphores.clear();

    swapchain.destroy(vkContext->device);

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(vkContext->instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    swapchainLayouts.clear();

    particleBuffer = nullptr;
    particleCount = 0;
    particlesConfigured = false;
    particleDescriptorSets.clear();

    fluidBuffer = nullptr;
    fluidSimWidth = 0;
    fluidSimHeight = 0;
    fluidConfigured = false;
    fluidDescriptorSet.clear();

    computeFinishedSemaphore = VK_NULL_HANDLE;
    currentFrame = 0;
    initialized = false;
}