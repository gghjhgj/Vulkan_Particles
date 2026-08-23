#include "FluidSystem.h"
#include "../../VK/VulkanContext.h"

#include <fstream>
#include <stdexcept>
#include <algorithm>

VkShaderModule FluidSystem::createShaderModule(VkDevice device, const std::string &path)
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
    createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

    VkShaderModule module{VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia ShaderModule dla: " + path);

    return module;
}

void FluidSystem::createComputePipeline(
    VkDevice device,
    VkShaderModule module,
    VkPipelineLayout pipelineLayout,
    VkPipeline &outPipeline)
{
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = module;
    pipelineInfo.stage.pName = "main";

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outPipeline) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia pipeline'u compute");
}

void FluidSystem::init(
    VulkanContext &context,
    uint32_t baseSimWidth,
    uint32_t baseSimHeight,
    const std::string &,
    uint32_t pushConstantSize)
{
    this->baseWidth = baseSimWidth;
    this->baseHeight = baseSimHeight;

    uint32_t sScale = std::max(1u, Config::fluid.simScale);
    uint32_t pScale = std::max(1u, Config::fluid.pressureScale);

    this->simWidth  = (baseSimWidth + sScale - 1) / sScale;
    this->simHeight = (baseSimHeight + sScale - 1) / sScale;

    this->pressWidth  = (baseSimWidth + pScale - 1) / pScale;
    this->pressHeight = (baseSimHeight + pScale - 1) / pScale;

    colorTextureA.init(context, simWidth, simHeight, VK_FORMAT_R8G8B8A8_UNORM);
    colorTextureB.init(context, simWidth, simHeight, VK_FORMAT_R8G8B8A8_UNORM);

    velocityTextureA.init(context, simWidth, simHeight, VK_FORMAT_R16G16_SFLOAT);
    velocityTextureB.init(context, simWidth, simHeight, VK_FORMAT_R16G16_SFLOAT);

    pressureTexture.init(context, pressWidth, pressHeight, VK_FORMAT_R16_SFLOAT);
    divergenceTexture.init(context, pressWidth, pressHeight, VK_FORMAT_R16_SFLOAT);

    linearSampler = VulkanTexture::createLinearClampSampler(context.device);

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

    VkCommandBufferBeginInfo initCmdBegin{};
    initCmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    initCmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &initCmdBegin);

    velocityTextureA.transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    velocityTextureB.transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    colorTextureA.transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    colorTextureB.transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    pressureTexture.transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    divergenceTexture.transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo initSubmit{};
    initSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    initSubmit.commandBufferCount = 1;
    initSubmit.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(context.computeQueue, 1, &initSubmit, VK_NULL_HANDLE);
    vkQueueWaitIdle(context.computeQueue);

    VkDescriptorSetLayoutBinding splatBindings[2]{};
    splatBindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    splatBindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo splatLayoutInfo{};
    splatLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    splatLayoutInfo.bindingCount = 2;
    splatLayoutInfo.pBindings = splatBindings;
    vkCreateDescriptorSetLayout(context.device, &splatLayoutInfo, nullptr, &splatDescriptorSetLayout);

    VkDescriptorSetLayoutBinding advectBindings[6]{};
    advectBindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[2] = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[5] = {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo advectLayoutInfo{};
    advectLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    advectLayoutInfo.bindingCount = 6;
    advectLayoutInfo.pBindings = advectBindings;
    vkCreateDescriptorSetLayout(context.device, &advectLayoutInfo, nullptr, &advectDescriptorSetLayout);

    VkDescriptorSetLayoutBinding jacobiBindings[2]{};
    jacobiBindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    jacobiBindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo jacobiLayoutInfo{};
    jacobiLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    jacobiLayoutInfo.bindingCount = 2;
    jacobiLayoutInfo.pBindings = jacobiBindings;
    vkCreateDescriptorSetLayout(context.device, &jacobiLayoutInfo, nullptr, &jacobiDescriptorSetLayout);

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = pushConstantSize;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pushConstantRangeCount = (pushConstantSize > 0) ? 1 : 0;
    pipelineLayoutInfo.pPushConstantRanges = (pushConstantSize > 0) ? &pushRange : nullptr;

    pipelineLayoutInfo.pSetLayouts = &splatDescriptorSetLayout;
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &splatPipelineLayout);

    pipelineLayoutInfo.pSetLayouts = &advectDescriptorSetLayout;
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &advectPipelineLayout);

    pipelineLayoutInfo.pSetLayouts = &jacobiDescriptorSetLayout;
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &jacobiPipelineLayout);

    VkShaderModule modSplat = createShaderModule(context.device, "shaders/fluids/fluid_splat.comp.spv");
    VkShaderModule modAdvect = createShaderModule(context.device, "shaders/fluids/fluid_advect_project.comp.spv");
    VkShaderModule modJacobi = createShaderModule(context.device, "shaders/fluids/fluid_jacobi.comp.spv");

    createComputePipeline(context.device, modSplat, splatPipelineLayout, pipelineSplat);
    createComputePipeline(context.device, modAdvect, advectPipelineLayout, pipelineAdvect);
    createComputePipeline(context.device, modJacobi, jacobiPipelineLayout, pipelineJacobi);

    vkDestroyShaderModule(context.device, modSplat, nullptr);
    vkDestroyShaderModule(context.device, modAdvect, nullptr);
    vkDestroyShaderModule(context.device, modJacobi, nullptr);

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 6;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 12;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 5;

    vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool);

    VkDescriptorSetLayout layouts[5] = {
        splatDescriptorSetLayout,
        splatDescriptorSetLayout,
        advectDescriptorSetLayout,
        advectDescriptorSetLayout,
        jacobiDescriptorSetLayout};

    VkDescriptorSet allSets[5]{};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 5;
    allocInfo.pSetLayouts = layouts;

    vkAllocateDescriptorSets(context.device, &allocInfo, allSets);

    splatDescriptorSets[0] = allSets[0];
    splatDescriptorSets[1] = allSets[1];
    advectDescriptorSets[0] = allSets[2];
    advectDescriptorSets[1] = allSets[3];
    jacobiDescriptorSet = allSets[4];

    VkDescriptorImageInfo velA_SamplerInfo{linearSampler, velocityTextureA.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo velB_SamplerInfo{linearSampler, velocityTextureB.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo velA_StorageInfo{VK_NULL_HANDLE, velocityTextureA.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo velB_StorageInfo{VK_NULL_HANDLE, velocityTextureB.view, VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorImageInfo colA_SamplerInfo{linearSampler, colorTextureA.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo colB_SamplerInfo{linearSampler, colorTextureB.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo colA_StorageInfo{VK_NULL_HANDLE, colorTextureA.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo colB_StorageInfo{VK_NULL_HANDLE, colorTextureB.view, VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorImageInfo press_SamplerInfo{linearSampler, pressureTexture.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo press_StorageInfo{VK_NULL_HANDLE, pressureTexture.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo div_StorageInfo{VK_NULL_HANDLE, divergenceTexture.view, VK_IMAGE_LAYOUT_GENERAL};

    VkWriteDescriptorSet splatWrites0[2]{};
    splatWrites0[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, splatDescriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &velA_StorageInfo, nullptr, nullptr};
    splatWrites0[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, splatDescriptorSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &colA_StorageInfo, nullptr, nullptr};

    VkWriteDescriptorSet splatWrites1[2]{};
    splatWrites1[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, splatDescriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &velB_StorageInfo, nullptr, nullptr};
    splatWrites1[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, splatDescriptorSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &colB_StorageInfo, nullptr, nullptr};

    VkWriteDescriptorSet advectWrites0[6]{};
    advectWrites0[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &press_SamplerInfo, nullptr, nullptr};
    advectWrites0[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &velA_SamplerInfo, nullptr, nullptr};
    advectWrites0[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &colA_SamplerInfo, nullptr, nullptr};
    advectWrites0[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &velB_StorageInfo, nullptr, nullptr};
    advectWrites0[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &colB_StorageInfo, nullptr, nullptr};
    advectWrites0[5] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &div_StorageInfo, nullptr, nullptr};

    VkWriteDescriptorSet advectWrites1[6]{};
    advectWrites1[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &press_SamplerInfo, nullptr, nullptr};
    advectWrites1[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &velB_SamplerInfo, nullptr, nullptr};
    advectWrites1[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &colB_SamplerInfo, nullptr, nullptr};
    advectWrites1[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &velA_StorageInfo, nullptr, nullptr};
    advectWrites1[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &colA_StorageInfo, nullptr, nullptr};
    advectWrites1[5] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &div_StorageInfo, nullptr, nullptr};

    VkWriteDescriptorSet jacobiWrites[2]{};
    jacobiWrites[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, jacobiDescriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &press_StorageInfo, nullptr, nullptr};
    jacobiWrites[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, jacobiDescriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &div_StorageInfo, nullptr, nullptr};

    VkWriteDescriptorSet allWrites[18]{};
    uint32_t writeIndex = 0;
    for (auto &write : splatWrites0) allWrites[writeIndex++] = write;
    for (auto &write : splatWrites1) allWrites[writeIndex++] = write;
    for (auto &write : advectWrites0) allWrites[writeIndex++] = write;
    for (auto &write : advectWrites1) allWrites[writeIndex++] = write;
    for (auto &write : jacobiWrites)  allWrites[writeIndex++] = write;

    vkUpdateDescriptorSets(context.device, writeIndex, allWrites, 0, nullptr);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(context.device, &fenceInfo, nullptr, &computeFence);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(context.device, &semInfo, nullptr, &computeFinishedSemaphore);
}

void FluidSystem::update(
    VulkanContext &context,
    const void *pushData,
    uint32_t pushConstantSize,
    VkSemaphore waitSemaphore)
{
    const uint32_t simGroupX   = (simWidth + 15) / 16;
    const uint32_t simGroupY   = (simHeight + 15) / 16;

    const uint32_t pressGroupX = (pressWidth + 15) / 16;
    const uint32_t pressGroupY = (pressHeight + 15) / 16;

    vkWaitForFences(context.device, 1, &computeFence, VK_TRUE, UINT64_MAX);
    vkResetFences(context.device, 1, &computeFence);
    vkResetCommandBuffer(commandBuffer, 0);

    const uint32_t currentInput  = colorPingPong;
    const uint32_t currentOutput = 1 - colorPingPong;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    auto insertComputeBarrier = [&](VkCommandBuffer cmd)
    {
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    };

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineJacobi);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, jacobiPipelineLayout, 0, 1, &jacobiDescriptorSet, 0, nullptr);

    if (pushData && pushConstantSize > 0)
        vkCmdPushConstants(commandBuffer, jacobiPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushData);

    for (uint32_t i = 0; i < pressureIterations; ++i)
    {
        vkCmdDispatch(commandBuffer, pressGroupX, pressGroupY, 1);
        insertComputeBarrier(commandBuffer);
    }

    bool isMouseDown = false;
    if (pushData && pushConstantSize >= sizeof(FluidPushConstants))
    {
        const auto *push = static_cast<const FluidPushConstants *>(pushData);
        isMouseDown = (push->isMouseDown != 0);
    }

    if (isMouseDown)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineSplat);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, splatPipelineLayout, 0, 1, &splatDescriptorSets[currentInput], 0, nullptr);

        if (pushData && pushConstantSize > 0)
            vkCmdPushConstants(commandBuffer, splatPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushData);

        vkCmdDispatch(commandBuffer, simGroupX, simGroupY, 1);
        insertComputeBarrier(commandBuffer);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineAdvect);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, advectPipelineLayout, 0, 1, &advectDescriptorSets[currentInput], 0, nullptr);

    if (pushData && pushConstantSize > 0)
        vkCmdPushConstants(commandBuffer, advectPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushData);

    vkCmdDispatch(commandBuffer, simGroupX, simGroupY, 1);

    VulkanTexture &activeColorTex = (currentOutput == 0) ? colorTextureA : colorTextureB;

    VkImageMemoryBarrier imageBarrier{};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.image = activeColorTex.image;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = 1;

    if (context.computeQueueFamilyIndex != context.graphicsQueueFamilyIndex)
    {
        imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imageBarrier.dstAccessMask = 0;
        imageBarrier.srcQueueFamilyIndex = context.computeQueueFamilyIndex;
        imageBarrier.dstQueueFamilyIndex = context.graphicsQueueFamilyIndex;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
    }
    else
    {
        imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
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

    colorPingPong = static_cast<uint8_t>(currentOutput);
}

void FluidSystem::destroy(VkDevice device)
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

    if (linearSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, linearSampler, nullptr);
        linearSampler = VK_NULL_HANDLE;
    }

    if (pipelineSplat != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipelineSplat, nullptr);
        pipelineSplat = VK_NULL_HANDLE;
    }

    if (pipelineAdvect != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipelineAdvect, nullptr);
        pipelineAdvect = VK_NULL_HANDLE;
    }

    if (pipelineJacobi != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipelineJacobi, nullptr);
        pipelineJacobi = VK_NULL_HANDLE;
    }

    if (splatPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, splatPipelineLayout, nullptr);
        splatPipelineLayout = VK_NULL_HANDLE;
    }

    if (advectPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, advectPipelineLayout, nullptr);
        advectPipelineLayout = VK_NULL_HANDLE;
    }

    if (jacobiPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, jacobiPipelineLayout, nullptr);
        jacobiPipelineLayout = VK_NULL_HANDLE;
    }

    if (splatDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, splatDescriptorSetLayout, nullptr);
        splatDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (advectDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, advectDescriptorSetLayout, nullptr);
        advectDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (jacobiDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, jacobiDescriptorSetLayout, nullptr);
        jacobiDescriptorSetLayout = VK_NULL_HANDLE;
    }

    velocityTextureA.destroy(device);
    velocityTextureB.destroy(device);
    colorTextureA.destroy(device);
    colorTextureB.destroy(device);
    pressureTexture.destroy(device);
    divergenceTexture.destroy(device);

    splatDescriptorSets[0] = splatDescriptorSets[1] = VK_NULL_HANDLE;
    advectDescriptorSets[0] = advectDescriptorSets[1] = VK_NULL_HANDLE;
    jacobiDescriptorSet = VK_NULL_HANDLE;
}