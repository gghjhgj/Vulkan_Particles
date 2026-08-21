#include "FluidSystem.h"
#include "../../VK/VulkanContext.h"

#include <fstream>
#include <stdexcept>

VkShaderModule FluidSystem::createShaderModule(VkDevice device, const std::string& path)
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

void FluidSystem::createComputePipeline(
    VkDevice device,
    VkShaderModule module,
    VkPipelineLayout pipelineLayout,
    VkPipeline& outPipeline)
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
    VulkanContext& context,
    uint32_t width,
    uint32_t height,
    const std::string&,
    uint32_t pushConstantSize)
{
    this->width = width;
    this->height = height;

    uint32_t pressWidth = (width + 1) / 2;
    uint32_t pressHeight = (height + 1) / 2;

    velocityTextureA.init(context, width, height, VK_FORMAT_R16G16_SFLOAT);
    velocityTextureB.init(context, width, height, VK_FORMAT_R16G16_SFLOAT);
   
    colorTextureA.init(context, width, height, VK_FORMAT_R8G8B8A8_UNORM);
    colorTextureB.init(context, width, height, VK_FORMAT_R8G8B8A8_UNORM);

    pressureTexture.init(context, pressWidth, pressHeight, VK_FORMAT_R32_SFLOAT);
    divergenceTexture.init(context, pressWidth, pressHeight, VK_FORMAT_R32_SFLOAT);

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

    VkDescriptorSetLayoutBinding advectBindings[5]{};
    advectBindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    advectBindings[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo advectLayoutInfo{};
    advectLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    advectLayoutInfo.bindingCount = 5;
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

    VkDescriptorSetLayoutBinding projectBindings[3]{};
    projectBindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    projectBindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    projectBindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo projectLayoutInfo{};
    projectLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    projectLayoutInfo.bindingCount = 3;
    projectLayoutInfo.pBindings = projectBindings;
    vkCreateDescriptorSetLayout(context.device, &projectLayoutInfo, nullptr, &projectDescriptorSetLayout);

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = pushConstantSize;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pushConstantRangeCount = (pushConstantSize > 0) ? 1 : 0;
    pipelineLayoutInfo.pPushConstantRanges = (pushConstantSize > 0) ? &pushRange : nullptr;

    pipelineLayoutInfo.pSetLayouts = &advectDescriptorSetLayout;
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &advectPipelineLayout);

    pipelineLayoutInfo.pSetLayouts = &jacobiDescriptorSetLayout;
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &jacobiPipelineLayout);

    pipelineLayoutInfo.pSetLayouts = &projectDescriptorSetLayout;
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &projectPipelineLayout);

    VkShaderModule modAdvect = createShaderModule(context.device, "shaders/fluids/fluid_advect.comp.spv");
    VkShaderModule modJacobi = createShaderModule(context.device, "shaders/fluids/fluid_jacobi.comp.spv");
    VkShaderModule modProject = createShaderModule(context.device, "shaders/fluids/fluid_project.comp.spv");

    createComputePipeline(context.device, modAdvect, advectPipelineLayout, pipelineAdvect);
    createComputePipeline(context.device, modJacobi, jacobiPipelineLayout, pipelineJacobi);
    createComputePipeline(context.device, modProject, projectPipelineLayout, pipelineProject);

    vkDestroyShaderModule(context.device, modAdvect, nullptr);
    vkDestroyShaderModule(context.device, modJacobi, nullptr);
    vkDestroyShaderModule(context.device, modProject, nullptr);

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 5;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 10;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 4;

    vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool);

    VkDescriptorSetLayout layouts[4] = {
        advectDescriptorSetLayout,
        advectDescriptorSetLayout,
        jacobiDescriptorSetLayout,
        projectDescriptorSetLayout
    };

    VkDescriptorSet allSets[4]{};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 4;
    allocInfo.pSetLayouts = layouts;

    vkAllocateDescriptorSets(context.device, &allocInfo, allSets);

    advectDescriptorSets[0] = allSets[0];
    advectDescriptorSets[1] = allSets[1];
    jacobiDescriptorSet     = allSets[2];
    projectDescriptorSet    = allSets[3];

    VkDescriptorImageInfo velA_SamplerInfo{linearSampler, velocityTextureA.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo velB_StorageInfo{VK_NULL_HANDLE, velocityTextureB.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo velA_StorageInfo{VK_NULL_HANDLE, velocityTextureA.view, VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorImageInfo colA_SamplerInfo{linearSampler, colorTextureA.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo colB_SamplerInfo{linearSampler, colorTextureB.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo colA_StorageInfo{VK_NULL_HANDLE, colorTextureA.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo colB_StorageInfo{VK_NULL_HANDLE, colorTextureB.view, VK_IMAGE_LAYOUT_GENERAL};

    VkDescriptorImageInfo press_StorageInfo{VK_NULL_HANDLE, pressureTexture.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo press_SamplerInfo{linearSampler, pressureTexture.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo div_StorageInfo{VK_NULL_HANDLE, divergenceTexture.view, VK_IMAGE_LAYOUT_GENERAL};

    VkWriteDescriptorSet advectWrites0[5]{};
    advectWrites0[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &velA_SamplerInfo, nullptr, nullptr};
    advectWrites0[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &colA_SamplerInfo, nullptr, nullptr};
    advectWrites0[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &velB_StorageInfo, nullptr, nullptr};
    advectWrites0[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &colB_StorageInfo, nullptr, nullptr};
    advectWrites0[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[0], 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &div_StorageInfo,  nullptr, nullptr};

    VkWriteDescriptorSet advectWrites1[5]{};
    advectWrites1[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &velA_SamplerInfo, nullptr, nullptr};
    advectWrites1[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &colB_SamplerInfo, nullptr, nullptr};
    advectWrites1[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &velB_StorageInfo, nullptr, nullptr};
    advectWrites1[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &colA_StorageInfo, nullptr, nullptr};
    advectWrites1[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, advectDescriptorSets[1], 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &div_StorageInfo,  nullptr, nullptr};

    VkWriteDescriptorSet jacobiWrites[2]{};
    jacobiWrites[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, jacobiDescriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &press_StorageInfo, nullptr, nullptr};
    jacobiWrites[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, jacobiDescriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &div_StorageInfo,   nullptr, nullptr};

    VkWriteDescriptorSet projectWrites[3]{};
    projectWrites[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, projectDescriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &press_SamplerInfo, nullptr, nullptr};
    projectWrites[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, projectDescriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &velB_StorageInfo,  nullptr, nullptr};
    projectWrites[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, projectDescriptorSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &velA_StorageInfo,  nullptr, nullptr};

    VkWriteDescriptorSet allWrites[15]{};
    uint32_t writeIndex = 0;
    for (auto& write : advectWrites0) allWrites[writeIndex++] = write;
    for (auto& write : advectWrites1) allWrites[writeIndex++] = write;
    for (auto& write : jacobiWrites)  allWrites[writeIndex++] = write;
    for (auto& write : projectWrites) allWrites[writeIndex++] = write;

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
    VulkanContext& context,
    const void* pushData,
    uint32_t pushConstantSize,
    VkSemaphore waitSemaphore)
{
    const uint32_t groupX = (width + 15) / 16;
    const uint32_t groupY = (height + 15) / 16;

    const uint32_t pressWidth = (width + 1) / 2;
    const uint32_t pressHeight = (height + 1) / 2;
    const uint32_t pressGroupX = (pressWidth + 15) / 16;
    const uint32_t pressGroupY = (pressHeight + 15) / 16;

    vkWaitForFences(context.device, 1, &computeFence, VK_TRUE, UINT64_MAX);
    vkResetFences(context.device, 1, &computeFence);
    vkResetCommandBuffer(commandBuffer, 0);

    const uint32_t colorInput = colorPingPong;
    const uint32_t colorOutput = 1 - colorPingPong;

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

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineAdvect);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        advectPipelineLayout,
        0, 1,
        &advectDescriptorSets[colorInput],
        0, nullptr);

    if (pushData && pushConstantSize > 0)
    {
        vkCmdPushConstants(commandBuffer, advectPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushData);
    }

    vkCmdDispatch(commandBuffer, groupX, groupY, 1);
    insertComputeBarrier(commandBuffer);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineJacobi);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        jacobiPipelineLayout,
        0, 1,
        &jacobiDescriptorSet,
        0, nullptr);

    if (pushData && pushConstantSize > 0)
    {
        vkCmdPushConstants(commandBuffer, jacobiPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushData);
    }

    for (uint32_t i = 0; i < pressureIterations; ++i)
    {
        vkCmdDispatch(commandBuffer, pressGroupX, pressGroupY, 1);
        insertComputeBarrier(commandBuffer);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineProject);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        projectPipelineLayout,
        0, 1,
        &projectDescriptorSet,
        0, nullptr);

    if (pushData && pushConstantSize > 0)
    {
        vkCmdPushConstants(commandBuffer, projectPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushData);
    }

    vkCmdDispatch(commandBuffer, groupX, groupY, 1);

    VulkanTexture& activeColorTex = (colorOutput == 0) ? colorTextureA : colorTextureB;

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

    colorPingPong = static_cast<uint8_t>(colorOutput);
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

    if (pipelineProject != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipelineProject, nullptr);
        pipelineProject = VK_NULL_HANDLE;
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

    if (projectPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, projectPipelineLayout, nullptr);
        projectPipelineLayout = VK_NULL_HANDLE;
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

    if (projectDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, projectDescriptorSetLayout, nullptr);
        projectDescriptorSetLayout = VK_NULL_HANDLE;
    }

    velocityTextureA.destroy(device);
    velocityTextureB.destroy(device);
    colorTextureA.destroy(device);
    colorTextureB.destroy(device);
    pressureTexture.destroy(device);
    divergenceTexture.destroy(device);

    advectDescriptorSets[0] = advectDescriptorSets[1] = VK_NULL_HANDLE;
    jacobiDescriptorSet = VK_NULL_HANDLE;
    projectDescriptorSet = VK_NULL_HANDLE;
}