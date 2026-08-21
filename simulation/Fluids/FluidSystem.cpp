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

    const uint32_t totalCells = width * height;

    velocityCells.resize(totalCells, {0.0f, 0.0f});
    colorCells.resize(totalCells, {0.0f, 0.0f, 0.0f, 0.0f});
    pressures.resize(totalCells, 0.0f);
    divergences.resize(totalCells, 0.0f);

    const VkDeviceSize velocityBufferSize = totalCells * sizeof(VelocityCell);
    const VkDeviceSize colorBufferSize = totalCells * sizeof(ColorCell);
    const VkDeviceSize scalarBufferSize = totalCells * sizeof(float);

    velocityBufferA.initStorage(context, velocityBufferSize, true);
    velocityBufferA.upload(velocityCells.data(), velocityBufferSize);

    velocityBufferB.initStorage(context, velocityBufferSize, true);
    velocityBufferB.upload(velocityCells.data(), velocityBufferSize);

    colorBufferA.initStorage(context, colorBufferSize, true);
    colorBufferA.upload(colorCells.data(), colorBufferSize);

    colorBufferB.initStorage(context, colorBufferSize, true);
    colorBufferB.upload(colorCells.data(), colorBufferSize);

    pressureBuffer.initStorage(context, scalarBufferSize, true);
    pressureBuffer.upload(pressures.data(), scalarBufferSize);

    divergenceBuffer.initStorage(context, scalarBufferSize, true);
    divergenceBuffer.upload(divergences.data(), scalarBufferSize);

    VkDescriptorSetLayoutBinding advectBindings[5]{};
    for (uint32_t i = 0; i < 5; ++i)
    {
        advectBindings[i].binding = i;
        advectBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        advectBindings[i].descriptorCount = 1;
        advectBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo advectLayoutInfo{};
    advectLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    advectLayoutInfo.bindingCount = 5;
    advectLayoutInfo.pBindings = advectBindings;
    vkCreateDescriptorSetLayout(context.device, &advectLayoutInfo, nullptr, &advectDescriptorSetLayout);

    VkDescriptorSetLayoutBinding jacobiBindings[2]{};
    for (uint32_t i = 0; i < 2; ++i)
    {
        jacobiBindings[i].binding = i;
        jacobiBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        jacobiBindings[i].descriptorCount = 1;
        jacobiBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo jacobiLayoutInfo{};
    jacobiLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    jacobiLayoutInfo.bindingCount = 2;
    jacobiLayoutInfo.pBindings = jacobiBindings;
    vkCreateDescriptorSetLayout(context.device, &jacobiLayoutInfo, nullptr, &jacobiDescriptorSetLayout);

    VkDescriptorSetLayoutBinding projectBindings[3]{};
    for (uint32_t i = 0; i < 3; ++i)
    {
        projectBindings[i].binding = i;
        projectBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        projectBindings[i].descriptorCount = 1;
        projectBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

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
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

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

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 15; 

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
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

    VkDescriptorBufferInfo velocityAInfo{velocityBufferA.handle, 0, velocityBufferA.size};
    VkDescriptorBufferInfo velocityBInfo{velocityBufferB.handle, 0, velocityBufferB.size};
    VkDescriptorBufferInfo colorAInfo{colorBufferA.handle, 0, colorBufferA.size};
    VkDescriptorBufferInfo colorBInfo{colorBufferB.handle, 0, colorBufferB.size};
    VkDescriptorBufferInfo pressureInfo{pressureBuffer.handle, 0, pressureBuffer.size};
    VkDescriptorBufferInfo divergenceInfo{divergenceBuffer.handle, 0, divergenceBuffer.size};

    VkWriteDescriptorSet advectWrites0[5]{};
    for (uint32_t i = 0; i < 5; ++i) {
        advectWrites0[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        advectWrites0[i].dstSet = advectDescriptorSets[0];
        advectWrites0[i].dstBinding = i;
        advectWrites0[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        advectWrites0[i].descriptorCount = 1;
    }
    advectWrites0[0].pBufferInfo = &velocityAInfo;
    advectWrites0[1].pBufferInfo = &velocityBInfo;
    advectWrites0[2].pBufferInfo = &colorAInfo;
    advectWrites0[3].pBufferInfo = &colorBInfo;
    advectWrites0[4].pBufferInfo = &divergenceInfo;

    VkWriteDescriptorSet advectWrites1[5]{};
    for (uint32_t i = 0; i < 5; ++i) {
        advectWrites1[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        advectWrites1[i].dstSet = advectDescriptorSets[1];
        advectWrites1[i].dstBinding = i;
        advectWrites1[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        advectWrites1[i].descriptorCount = 1;
    }
    advectWrites1[0].pBufferInfo = &velocityAInfo;
    advectWrites1[1].pBufferInfo = &velocityBInfo;
    advectWrites1[2].pBufferInfo = &colorBInfo;
    advectWrites1[3].pBufferInfo = &colorAInfo;
    advectWrites1[4].pBufferInfo = &divergenceInfo;

    VkWriteDescriptorSet jacobiWrites[2]{};
    for (uint32_t i = 0; i < 2; ++i) {
        jacobiWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        jacobiWrites[i].dstSet = jacobiDescriptorSet;
        jacobiWrites[i].dstBinding = i;
        jacobiWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        jacobiWrites[i].descriptorCount = 1;
    }
    jacobiWrites[0].pBufferInfo = &pressureInfo;
    jacobiWrites[1].pBufferInfo = &divergenceInfo;

    VkWriteDescriptorSet projectWrites[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        projectWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        projectWrites[i].dstSet = projectDescriptorSet;
        projectWrites[i].dstBinding = i;
        projectWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        projectWrites[i].descriptorCount = 1;
    }
    projectWrites[0].pBufferInfo = &pressureInfo;
    projectWrites[1].pBufferInfo = &velocityBInfo;
    projectWrites[2].pBufferInfo = &velocityAInfo;

    VkWriteDescriptorSet allWrites[15]{};
    uint32_t writeIndex = 0;
    for (auto& write : advectWrites0) allWrites[writeIndex++] = write;
    for (auto& write : advectWrites1) allWrites[writeIndex++] = write;
    for (auto& write : jacobiWrites)  allWrites[writeIndex++] = write;
    for (auto& write : projectWrites) allWrites[writeIndex++] = write;

    vkUpdateDescriptorSets(context.device, writeIndex, allWrites, 0, nullptr);

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

void FluidSystem::update(
    VulkanContext& context,
    const void* pushData,
    uint32_t pushConstantSize,
    VkSemaphore waitSemaphore)
{
    const uint32_t groupX = (width + 15) / 16;
    const uint32_t groupY = (height + 15) / 16;

    const uint32_t rbGroupX = ((width / 2) + 15) / 16;

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
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr);
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

    const uint32_t phaseOffset = pushConstantSize - sizeof(uint32_t);

    for (uint32_t i = 0; i < pressureIterations; ++i)
    {
        uint32_t phase = i & 1u;

        vkCmdPushConstants(commandBuffer, jacobiPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, phaseOffset, sizeof(uint32_t), &phase);
        vkCmdDispatch(commandBuffer, rbGroupX, groupY, 1);
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

    VkBuffer activeColorBuffer = (colorOutput == 0) ? colorBufferA.handle : colorBufferB.handle;
    VkBufferMemoryBarrier bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.buffer = activeColorBuffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;

    if (context.computeQueueFamilyIndex != context.graphicsQueueFamilyIndex)
    {
        bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bufferBarrier.dstAccessMask = 0;
        bufferBarrier.srcQueueFamilyIndex = context.computeQueueFamilyIndex;
        bufferBarrier.dstQueueFamilyIndex = context.graphicsQueueFamilyIndex;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }
    else
    {
        bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
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

    velocityBufferA.destroy(device);
    velocityBufferB.destroy(device);
    colorBufferA.destroy(device);
    colorBufferB.destroy(device);
    pressureBuffer.destroy(device);
    divergenceBuffer.destroy(device);

    advectDescriptorSets[0] = advectDescriptorSets[1] = VK_NULL_HANDLE;
    jacobiDescriptorSet = VK_NULL_HANDLE;
    projectDescriptorSet = VK_NULL_HANDLE;
}