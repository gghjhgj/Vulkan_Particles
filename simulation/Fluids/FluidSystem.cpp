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

    velocityCells.resize(totalCells);
    colorCells.resize(totalCells);
    pressures.resize(totalCells);
    divergences.resize(totalCells);

    for (uint32_t i = 0; i < totalCells; ++i)
    {
        velocityCells[i] = {0.0f, 0.0f};
        colorCells[i] = {0.0f, 0.0f, 0.0f, 0.0f};
        pressures[i] = 0.0f;
        divergences[i] = 0.0f;
    }

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

    pressureBufferA.initStorage(context, scalarBufferSize, true);
    pressureBufferA.upload(pressures.data(), scalarBufferSize);

    pressureBufferB.initStorage(context, scalarBufferSize, true);
    pressureBufferB.upload(pressures.data(), scalarBufferSize);

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

    if (vkCreateDescriptorSetLayout(
            context.device,
            &advectLayoutInfo,
            nullptr,
            &advectDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Blad tworzenia advectDescriptorSetLayout");
    }

    VkDescriptorSetLayoutBinding jacobiBindings[3]{};
    for (uint32_t i = 0; i < 3; ++i)
    {
        jacobiBindings[i].binding = i;
        jacobiBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        jacobiBindings[i].descriptorCount = 1;
        jacobiBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo jacobiLayoutInfo{};
    jacobiLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    jacobiLayoutInfo.bindingCount = 3;
    jacobiLayoutInfo.pBindings = jacobiBindings;

    if (vkCreateDescriptorSetLayout(
            context.device,
            &jacobiLayoutInfo,
            nullptr,
            &jacobiDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Blad tworzenia jacobiDescriptorSetLayout");
    }

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

    if (vkCreateDescriptorSetLayout(
            context.device,
            &projectLayoutInfo,
            nullptr,
            &projectDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Blad tworzenia projectDescriptorSetLayout");
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = pushConstantSize;

    VkPipelineLayoutCreateInfo advectPipelineLayoutInfo{};
    advectPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    advectPipelineLayoutInfo.setLayoutCount = 1;
    advectPipelineLayoutInfo.pSetLayouts = &advectDescriptorSetLayout;
    advectPipelineLayoutInfo.pushConstantRangeCount = 1;
    advectPipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(context.device, &advectPipelineLayoutInfo, nullptr, &advectPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia advectPipelineLayout");

    VkPipelineLayoutCreateInfo jacobiPipelineLayoutInfo{};
    jacobiPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    jacobiPipelineLayoutInfo.setLayoutCount = 1;
    jacobiPipelineLayoutInfo.pSetLayouts = &jacobiDescriptorSetLayout;
    jacobiPipelineLayoutInfo.pushConstantRangeCount = 1;
    jacobiPipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(context.device, &jacobiPipelineLayoutInfo, nullptr, &jacobiPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia jacobiPipelineLayout");

    VkPipelineLayoutCreateInfo projectPipelineLayoutInfo{};
    projectPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    projectPipelineLayoutInfo.setLayoutCount = 1;
    projectPipelineLayoutInfo.pSetLayouts = &projectDescriptorSetLayout;
    projectPipelineLayoutInfo.pushConstantRangeCount = 1;
    projectPipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(context.device, &projectPipelineLayoutInfo, nullptr, &projectPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia projectPipelineLayout");

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
    poolSize.descriptorCount = 22;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 6;

    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia descriptorPool");

    VkDescriptorSetLayout layouts[6] = {
        advectDescriptorSetLayout,
        advectDescriptorSetLayout,
        jacobiDescriptorSetLayout,
        jacobiDescriptorSetLayout,
        projectDescriptorSetLayout,
        projectDescriptorSetLayout
    };

    VkDescriptorSet allSets[6]{};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 6;
    allocInfo.pSetLayouts = layouts;

    if (vkAllocateDescriptorSets(context.device, &allocInfo, allSets) != VK_SUCCESS)
        throw std::runtime_error("Blad alokacji descriptorSets");

    advectDescriptorSets[0] = allSets[0];
    advectDescriptorSets[1] = allSets[1];
    jacobiDescriptorSets[0] = allSets[2];
    jacobiDescriptorSets[1] = allSets[3];
    projectDescriptorSets[0] = allSets[4];
    projectDescriptorSets[1] = allSets[5];

    VkDescriptorBufferInfo velocityAInfo{velocityBufferA.handle, 0, velocityBufferA.size};
    VkDescriptorBufferInfo velocityBInfo{velocityBufferB.handle, 0, velocityBufferB.size};
    VkDescriptorBufferInfo colorAInfo{colorBufferA.handle, 0, colorBufferA.size};
    VkDescriptorBufferInfo colorBInfo{colorBufferB.handle, 0, colorBufferB.size};
    VkDescriptorBufferInfo pressureAInfo{pressureBufferA.handle, 0, pressureBufferA.size};
    VkDescriptorBufferInfo pressureBInfo{pressureBufferB.handle, 0, pressureBufferB.size};
    VkDescriptorBufferInfo divergenceInfo{divergenceBuffer.handle, 0, divergenceBuffer.size};

    VkWriteDescriptorSet advectWrites0[5]{};
    for (uint32_t i = 0; i < 5; ++i)
    {
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
    for (uint32_t i = 0; i < 5; ++i)
    {
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

    VkWriteDescriptorSet jacobiWrites0[3]{};
    for (uint32_t i = 0; i < 3; ++i)
    {
        jacobiWrites0[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        jacobiWrites0[i].dstSet = jacobiDescriptorSets[0];
        jacobiWrites0[i].dstBinding = i;
        jacobiWrites0[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        jacobiWrites0[i].descriptorCount = 1;
    }
    jacobiWrites0[0].pBufferInfo = &pressureAInfo;
    jacobiWrites0[1].pBufferInfo = &pressureBInfo;
    jacobiWrites0[2].pBufferInfo = &divergenceInfo;

    VkWriteDescriptorSet jacobiWrites1[3]{};
    for (uint32_t i = 0; i < 3; ++i)
    {
        jacobiWrites1[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        jacobiWrites1[i].dstSet = jacobiDescriptorSets[1];
        jacobiWrites1[i].dstBinding = i;
        jacobiWrites1[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        jacobiWrites1[i].descriptorCount = 1;
    }
    jacobiWrites1[0].pBufferInfo = &pressureBInfo;
    jacobiWrites1[1].pBufferInfo = &pressureAInfo;
    jacobiWrites1[2].pBufferInfo = &divergenceInfo;

    VkWriteDescriptorSet projectWrites0[3]{};
    for (uint32_t i = 0; i < 3; ++i)
    {
        projectWrites0[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        projectWrites0[i].dstSet = projectDescriptorSets[0];
        projectWrites0[i].dstBinding = i;
        projectWrites0[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        projectWrites0[i].descriptorCount = 1;
    }
    projectWrites0[0].pBufferInfo = &pressureAInfo;
    projectWrites0[1].pBufferInfo = &velocityBInfo;
    projectWrites0[2].pBufferInfo = &velocityAInfo;

    VkWriteDescriptorSet projectWrites1[3]{};
    for (uint32_t i = 0; i < 3; ++i)
    {
        projectWrites1[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        projectWrites1[i].dstSet = projectDescriptorSets[1];
        projectWrites1[i].dstBinding = i;
        projectWrites1[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        projectWrites1[i].descriptorCount = 1;
    }
    projectWrites1[0].pBufferInfo = &pressureBInfo;
    projectWrites1[1].pBufferInfo = &velocityBInfo;
    projectWrites1[2].pBufferInfo = &velocityAInfo;

    VkWriteDescriptorSet allWrites[22]{};
    uint32_t writeIndex = 0;

    for (auto& write : advectWrites0) allWrites[writeIndex++] = write;
    for (auto& write : advectWrites1) allWrites[writeIndex++] = write;
    for (auto& write : jacobiWrites0) allWrites[writeIndex++] = write;
    for (auto& write : jacobiWrites1) allWrites[writeIndex++] = write;
    for (auto& write : projectWrites0) allWrites[writeIndex++] = write;
    for (auto& write : projectWrites1) allWrites[writeIndex++] = write;

    vkUpdateDescriptorSets(context.device, writeIndex, allWrites, 0, nullptr);

    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolInfo.queueFamilyIndex = context.computeQueueFamilyIndex;

    if (vkCreateCommandPool(context.device, &cmdPoolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia commandPool");

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(context.device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("Blad alokacji commandBuffer");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(context.device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia computeFence");

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(context.device, &semInfo, nullptr, &computeFinishedSemaphore) != VK_SUCCESS)
        throw std::runtime_error("Blad tworzenia computeFinishedSemaphore");
}

void FluidSystem::update(
    VulkanContext& context,
    const void* pushData,
    uint32_t pushConstantSize,
    VkSemaphore waitSemaphore)
{
    const uint32_t groupX = (width + 15) / 16;
    const uint32_t groupY = (height + 15) / 16;

    vkWaitForFences(context.device, 1, &computeFence, VK_TRUE, UINT64_MAX);
    vkResetFences(context.device, 1, &computeFence);
    vkResetCommandBuffer(commandBuffer, 0);

    const uint32_t colorInput = colorPingPong;
    const uint32_t colorOutput = 1 - colorPingPong;
    const uint32_t advectSetIndex = colorInput;
    
    const uint32_t finalPressureIndex = pressureIterations & 1u;
    const uint32_t projectSetIndex = finalPressureIndex;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    vkCmdFillBuffer(commandBuffer, pressureBufferA.handle, 0, VK_WHOLE_SIZE, 0);

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
        &advectDescriptorSets[advectSetIndex],
        0, nullptr);

    if (pushData && pushConstantSize > 0)
    {
        vkCmdPushConstants(
            commandBuffer,
            advectPipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, pushConstantSize, pushData);
    }

    vkCmdDispatch(commandBuffer, groupX, groupY, 1);
    insertComputeBarrier(commandBuffer);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineJacobi);

    if (pushData && pushConstantSize > 0)
    {
        vkCmdPushConstants(
            commandBuffer,
            jacobiPipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, pushConstantSize, pushData);
    }

    for (uint32_t i = 0; i < pressureIterations; ++i)
    {
        const uint32_t jacobiSetIndex = i & 1u;

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            jacobiPipelineLayout,
            0, 1,
            &jacobiDescriptorSets[jacobiSetIndex],
            0, nullptr);

        vkCmdDispatch(commandBuffer, groupX, groupY, 1);
        insertComputeBarrier(commandBuffer);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineProject);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        projectPipelineLayout,
        0, 1,
        &projectDescriptorSets[projectSetIndex],
        0, nullptr);

    if (pushData && pushConstantSize > 0)
    {
        vkCmdPushConstants(
            commandBuffer,
            projectPipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, pushConstantSize, pushData);
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

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr,
            1, &bufferBarrier,
            0, nullptr);
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
            0, 0, nullptr,
            1, &bufferBarrier,
            0, nullptr);
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
    pressureBufferA.destroy(device);
    pressureBufferB.destroy(device);
    divergenceBuffer.destroy(device);

    advectDescriptorSets[0] = advectDescriptorSets[1] = VK_NULL_HANDLE;
    jacobiDescriptorSets[0] = jacobiDescriptorSets[1] = VK_NULL_HANDLE;
    projectDescriptorSets[0] = projectDescriptorSets[1] = VK_NULL_HANDLE;
}