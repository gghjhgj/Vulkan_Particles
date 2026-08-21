#include "FluidParticles.h"
#include "../Particles/ParticleSystem.h"
#include "../Fluids/FluidSystem.h"

#include <fstream>
#include <stdexcept>
#include <iostream>

VkShaderModule FluidParticles::createShaderModule(VkDevice device, const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Nie udalo sie otworzyc shadera: " + path);

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule module{VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia ShaderModule dla: " + path);

    return module;
}

void FluidParticles::init(
    VulkanContext& context,
    ParticleSystem& particles,
    FluidSystem& fluid,
    const std::string& shaderPath,
    uint32_t pushConstantSize
)
{
    particleCount = particles.getCount();

    // 1. Tworzenie Layoutu Deskryptorów:
    // Binding 0: Bufor cząsteczek (STORAGE_BUFFER)
    // Binding 1: Prędkość płynu (STORAGE_IMAGE)
    // Binding 2: Kolor płynu (STORAGE_IMAGE)
    VkDescriptorSetLayoutBinding bindings[3]{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia descriptorSetLayout w FluidParticles");

    // 2. Pipeline Layout z Push Constantami
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = pushConstantSize;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = (pushConstantSize > 0) ? 1 : 0;
    pipelineLayoutInfo.pPushConstantRanges = (pushConstantSize > 0) ? &pushRange : nullptr;

    if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia pipelineLayout w FluidParticles");

    // 3. Tworzenie Pipeline'u Compute ze Specialization Constants
    VkShaderModule shaderModule = createShaderModule(context.device, shaderPath);

    uint32_t specializationData[2] = { 256, particleCount };
    VkSpecializationMapEntry specMapEntries[2]{};
    specMapEntries[0] = {0, 0, sizeof(uint32_t)};
    specMapEntries[1] = {1, sizeof(uint32_t), sizeof(uint32_t)};

    VkSpecializationInfo specInfo{};
    specInfo.mapEntryCount = 2;
    specInfo.pMapEntries = specMapEntries;
    specInfo.dataSize = sizeof(specializationData);
    specInfo.pData = specializationData;

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.stage.pSpecializationInfo = &specInfo;

    if (vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia compute pipeline w FluidParticles");

    vkDestroyShaderModule(context.device, shaderModule, nullptr);

    // 4. Descriptor Pool (2 bufory SSBO, 4 obrazy STORAGE_IMAGE dla 2 setów)
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  4};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 2;

    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia descriptorPool w FluidParticles");

    VkDescriptorSetLayout layouts[2] = { descriptorSetLayout, descriptorSetLayout };
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts;

    if (vkAllocateDescriptorSets(context.device, &allocInfo, descriptorSets) != VK_SUCCESS)
        throw std::runtime_error("Blad alokacji descriptorSets w FluidParticles");

    // 5. Konfiguracja i zapis Deskryptorów dla Set 0 (Ping-Pong A) i Set 1 (Ping-Pong B)
    VkDescriptorBufferInfo particleBufInfo{ particles.getBuffer().handle, 0, particles.getBuffer().size };

    VkDescriptorImageInfo velAInfo{ VK_NULL_HANDLE, fluid.getVelocityTextureA().view, VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo colAInfo{ VK_NULL_HANDLE, fluid.getColorTextureA().view,    VK_IMAGE_LAYOUT_GENERAL };

    VkDescriptorImageInfo velBInfo{ VK_NULL_HANDLE, fluid.getVelocityTextureB().view, VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo colBInfo{ VK_NULL_HANDLE, fluid.getColorTextureB().view,    VK_IMAGE_LAYOUT_GENERAL };

    VkWriteDescriptorSet writes[6]{};

    // Set 0 (Ping-Pong A)
    writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &particleBufInfo, nullptr};
    writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &velAInfo, nullptr, nullptr};
    writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &colAInfo, nullptr, nullptr};

    // Set 1 (Ping-Pong B)
    writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &particleBufInfo, nullptr};
    writes[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &velBInfo, nullptr, nullptr};
    writes[5] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &colBInfo, nullptr, nullptr};

    vkUpdateDescriptorSets(context.device, 6, writes, 0, nullptr);

    // 6. Command Pool, Command Buffer, Fences i Semaphores
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolInfo.queueFamilyIndex = context.computeQueueFamilyIndex;
    vkCreateCommandPool(context.device, &cmdPoolInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(context.device, &cmdAllocInfo, &commandBuffer);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(context.device, &fenceInfo, nullptr, &computeFence);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(context.device, &semInfo, nullptr, &computeFinishedSemaphore);
}

void FluidParticles::update(
    VulkanContext& context,
    ParticleSystem& particles,
    FluidSystem& fluid,
    const void* pushData,
    uint32_t pushConstantSize,
    VkSemaphore waitSemaphore
)
{
    constexpr uint32_t WORKGROUP_SIZE = 256;
    uint32_t groupCount = (particleCount + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    uint32_t activeSetIndex = fluid.getColorPingPong();

    vkWaitForFences(context.device, 1, &computeFence, VK_TRUE, UINT64_MAX);
    vkResetFences(context.device, 1, &computeFence);
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipelineLayout,
        0, 1,
        &descriptorSets[activeSetIndex],
        0, nullptr);

    if (pushData && pushConstantSize > 0)
    {
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, pushConstantSize, pushData);
    }

    vkCmdDispatch(commandBuffer, groupCount, 1, 1);

    // Bariera bufora cząsteczek
    VkBufferMemoryBarrier bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.buffer = particles.getBuffer().handle;
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;

    if (context.computeQueueFamilyIndex != context.graphicsQueueFamilyIndex)
    {
        bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bufferBarrier.dstAccessMask = 0;
        bufferBarrier.srcQueueFamilyIndex = context.computeQueueFamilyIndex;
        bufferBarrier.dstQueueFamilyIndex = context.graphicsQueueFamilyIndex;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
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
            0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (waitSemaphore != VK_NULL_HANDLE)
    {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
    }

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &computeFinishedSemaphore;

    vkQueueSubmit(context.computeQueue, 1, &submitInfo, computeFence);
}

void FluidParticles::destroy(VkDevice device)
{
    if (device == VK_NULL_HANDLE)
        return;

    if (computeFence != VK_NULL_HANDLE)
    {
        vkWaitForFences(device, 1, &computeFence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device, computeFence, nullptr);
        computeFence = VK_NULL_HANDLE;
    }

    if (computeFinishedSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(device, computeFinishedSemaphore, nullptr);
        computeFinishedSemaphore = VK_NULL_HANDLE;
    }

    if (commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
        commandBuffer = VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }

    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    descriptorSets[0] = VK_NULL_HANDLE;
    descriptorSets[1] = VK_NULL_HANDLE;
}