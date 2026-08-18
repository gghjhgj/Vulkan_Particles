#include "VulkanComputePipeline.h"
#include "VulkanContext.h"
#include "VulkanBuffer.h"

std::vector<char> VulkanComputePipeline::readFile(
    const std::string &filename)
{
    std::ifstream file(
        filename,
        std::ios::ate | std::ios::ios_base::binary);

    if (!file.is_open())
    {
        throw std::runtime_error(
            "error: couldn't open shader file: " +
            filename);
    }

    size_t fileSize =
        (size_t)file.tellg();

    std::vector<char> buffer(fileSize);

    file.seekg(0);

    file.read(
        buffer.data(),
        fileSize);

    file.close();

    return buffer;
}

VkShaderModule VulkanComputePipeline::createShaderModule(
    VkDevice device,
    const std::vector<char> &code)
{
    VkShaderModuleCreateInfo createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    createInfo.codeSize =
        code.size();

    createInfo.pCode =
        reinterpret_cast<const uint32_t *>(
            code.data());

    VkShaderModule module;

    if (vkCreateShaderModule(
            device,
            &createInfo,
            nullptr,
            &module) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't open shader module");
    }

    return module;
}

void VulkanComputePipeline::init(
    const VulkanContext &context,
    const std::string &shaderPath,
    uint32_t pushConstantSize,
    uint32_t workGroupSizeX,
    uint32_t bindingCount,
    uint32_t specializationConstant)
{
    auto shaderCode =
        readFile(shaderPath);

    shaderModule =
        createShaderModule(
            context.device,
            shaderCode);

    std::vector<VkDescriptorSetLayoutBinding>
        layoutBindings(bindingCount);

    for (uint32_t i = 0;
         i < bindingCount;
         ++i)
    {
        layoutBindings[i].binding = i;

        layoutBindings[i].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

        layoutBindings[i].descriptorCount = 1;

        layoutBindings[i].stageFlags =
            VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};

    layoutInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    layoutInfo.bindingCount =
        bindingCount;

    layoutInfo.pBindings =
        layoutBindings.data();

    if (vkCreateDescriptorSetLayout(
            context.device,
            &layoutInfo,
            nullptr,
            &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't create descriptor set layout");
    }

    VkPushConstantRange pushConstantRange{};

    if (pushConstantSize > 0)
    {
        pushConstantRange.stageFlags =
            VK_SHADER_STAGE_COMPUTE_BIT;

        pushConstantRange.offset = 0;

        pushConstantRange.size =
            pushConstantSize;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};

    pipelineLayoutInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    pipelineLayoutInfo.setLayoutCount = 1;

    pipelineLayoutInfo.pSetLayouts =
        &descriptorSetLayout;

    if (pushConstantSize > 0)
    {
        pipelineLayoutInfo.pushConstantRangeCount =
            1;

        pipelineLayoutInfo.pPushConstantRanges =
            &pushConstantRange;
    }
    else
    {
        pipelineLayoutInfo.pushConstantRangeCount =
            0;

        pipelineLayoutInfo.pPushConstantRanges =
            nullptr;
    }

    if (vkCreatePipelineLayout(
            context.device,
            &pipelineLayoutInfo,
            nullptr,
            &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "create: couldn't create pipeline layout");
    }

    VkComputePipelineCreateInfo pipelineInfo{};

    pipelineInfo.sType =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

    pipelineInfo.layout =
        pipelineLayout;

    pipelineInfo.stage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    pipelineInfo.stage.stage =
        VK_SHADER_STAGE_COMPUTE_BIT;

    pipelineInfo.stage.module =
        shaderModule;

    pipelineInfo.stage.pName =
        "main";

    uint32_t specializationData[2] =
        {
            workGroupSizeX,
            specializationConstant
        };

    VkSpecializationMapEntry specMapEntries[2]{};

    specMapEntries[0].constantID = 0;
    specMapEntries[0].offset = 0;
    specMapEntries[0].size = sizeof(uint32_t);

    specMapEntries[1].constantID = 1;
    specMapEntries[1].offset = sizeof(uint32_t);
    specMapEntries[1].size = sizeof(uint32_t);

    VkSpecializationInfo specInfo{};

    specInfo.mapEntryCount = 2;
    specInfo.pMapEntries = specMapEntries;

    specInfo.dataSize =
        sizeof(specializationData);

    specInfo.pData =
        specializationData;

    pipelineInfo.stage.pSpecializationInfo =
        &specInfo;

    if (vkCreateComputePipelines(
            context.device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't create compute pipeline");
    }

    VkDescriptorPoolSize poolSize{};

    poolSize.type =
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    poolSize.descriptorCount =
        bindingCount;

    VkDescriptorPoolCreateInfo poolInfo{};

    poolInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    poolInfo.poolSizeCount = 1;

    poolInfo.pPoolSizes =
        &poolSize;

    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(
            context.device,
            &poolInfo,
            nullptr,
            &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't create descriptor pool");
    }

    VkDescriptorSetAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    allocInfo.descriptorPool =
        descriptorPool;

    allocInfo.descriptorSetCount = 1;

    allocInfo.pSetLayouts =
        &descriptorSetLayout;

    if (vkAllocateDescriptorSets(
            context.device,
            &allocInfo,
            &descriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't allocate descriptor set");
    }

    VkCommandPoolCreateInfo cmdPoolInfo{};

    cmdPoolInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    cmdPoolInfo.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    cmdPoolInfo.queueFamilyIndex =
        context.computeQueueFamilyIndex;

    if (vkCreateCommandPool(
            context.device,
            &cmdPoolInfo,
            nullptr,
            &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't create command pool");
    }

    VkCommandBufferAllocateInfo cmdAllocInfo{};

    cmdAllocInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    cmdAllocInfo.commandPool =
        commandPool;

    cmdAllocInfo.level =
        VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    cmdAllocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(
            context.device,
            &cmdAllocInfo,
            &commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't allocate compute command buffer");
    }

    VkFenceCreateInfo fenceInfo{};

    fenceInfo.sType =
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    fenceInfo.flags =
        VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(
            context.device,
            &fenceInfo,
            nullptr,
            &computeFence) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't create compute fence");
    }

    VkSemaphoreCreateInfo semaphoreInfo{};

    semaphoreInfo.sType =
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(
            context.device,
            &semaphoreInfo,
            nullptr,
            &computeFinishedSemaphore) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't create compute finished semaphore");
    }
}

void VulkanComputePipeline::bindBuffers(
    const VulkanContext &context,
    const std::vector<VulkanBuffer> &buffers)
{
    std::vector<VkDescriptorBufferInfo>
        bufferInfos(buffers.size());

    std::vector<VkWriteDescriptorSet>
        descriptorWrites(buffers.size());

    for (size_t i = 0;
         i < buffers.size();
         ++i)
    {
        bufferInfos[i].buffer =
            buffers[i].handle;

        bufferInfos[i].offset =
            0;

        bufferInfos[i].range =
            buffers[i].size;

        descriptorWrites[i].sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

        descriptorWrites[i].pNext =
            nullptr;

        descriptorWrites[i].dstSet =
            descriptorSet;

        descriptorWrites[i].dstBinding =
            static_cast<uint32_t>(i);

        descriptorWrites[i].dstArrayElement =
            0;

        descriptorWrites[i].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

        descriptorWrites[i].descriptorCount =
            1;

        descriptorWrites[i].pBufferInfo =
            &bufferInfos[i];

        descriptorWrites[i].pImageInfo =
            nullptr;

        descriptorWrites[i].pTexelBufferView =
            nullptr;
    }

    vkUpdateDescriptorSets(
        context.device,
        static_cast<uint32_t>(
            descriptorWrites.size()),
        descriptorWrites.data(),
        0,
        nullptr);
}

void VulkanComputePipeline::dispatch(
    const VulkanContext &context,
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ,
    const void *pushConstantData,
    uint32_t pushConstantSize,
    VkBuffer particleBuffer)
{
    if (vkWaitForFences(
            context.device,
            1,
            &computeFence,
            VK_TRUE,
            UINT64_MAX) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't wait for compute fence");
    }

    if (vkResetFences(
            context.device,
            1,
            &computeFence) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't reset compute fence");
    }

    vkResetCommandBuffer(
        commandBuffer,
        0);

    VkCommandBufferBeginInfo beginInfo{};

    beginInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    beginInfo.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(
            commandBuffer,
            &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't begin compute command buffer");
    }

    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline);

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipelineLayout,
        0,
        1,
        &descriptorSet,
        0,
        nullptr);

    if (pushConstantData && pushConstantSize > 0)
    {
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            pushConstantSize,
            pushConstantData);
    }

    vkCmdDispatch(
        commandBuffer,
        groupCountX,
        groupCountY,
        groupCountZ);

    VkBufferMemoryBarrier bufferBarrier{};

    bufferBarrier.sType =
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

    bufferBarrier.buffer =
        particleBuffer;

    bufferBarrier.offset = 0;

    bufferBarrier.size =
        VK_WHOLE_SIZE;

    if (context.computeQueueFamilyIndex !=
        context.graphicsQueueFamilyIndex)
    {
        bufferBarrier.srcAccessMask =
            VK_ACCESS_SHADER_WRITE_BIT;

        bufferBarrier.dstAccessMask = 0;

        bufferBarrier.srcQueueFamilyIndex =
            context.computeQueueFamilyIndex;

        bufferBarrier.dstQueueFamilyIndex =
            context.graphicsQueueFamilyIndex;

        vkCmdPipelineBarrier(
            commandBuffer,

            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,

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
        bufferBarrier.srcAccessMask =
            VK_ACCESS_SHADER_WRITE_BIT;

        bufferBarrier.dstAccessMask =
            VK_ACCESS_SHADER_READ_BIT;

        bufferBarrier.srcQueueFamilyIndex =
            VK_QUEUE_FAMILY_IGNORED;

        bufferBarrier.dstQueueFamilyIndex =
            VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(
            commandBuffer,

            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,

            0,

            0,
            nullptr,

            1,
            &bufferBarrier,

            0,
            nullptr);
    }

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't end compute command buffer");
    }

    VkSubmitInfo submitInfo{};

    submitInfo.sType =
        VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = 1;

    submitInfo.pCommandBuffers =
        &commandBuffer;

    submitInfo.signalSemaphoreCount = 1;

    submitInfo.pSignalSemaphores =
        &computeFinishedSemaphore;

    if (vkQueueSubmit(
            context.computeQueue,
            1,
            &submitInfo,
            computeFence) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "error: couldn't submit compute command buffer");
    }
}
void VulkanComputePipeline::destroy(
    VkDevice device)
{
    if (device == VK_NULL_HANDLE)
        return;

    if (computeFence != VK_NULL_HANDLE)
    {
        vkWaitForFences(
            device,
            1,
            &computeFence,
            VK_TRUE,
            UINT64_MAX);
    }

    if (computeFinishedSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(
            device,
            computeFinishedSemaphore,
            nullptr);

        computeFinishedSemaphore =
            VK_NULL_HANDLE;
    }

    if (computeFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(
            device,
            computeFence,
            nullptr);

        computeFence =
            VK_NULL_HANDLE;
    }

    if (commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(
            device,
            commandPool,
            nullptr);

        commandPool =
            VK_NULL_HANDLE;

        commandBuffer =
            VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(
            device,
            descriptorPool,
            nullptr);

        descriptorPool =
            VK_NULL_HANDLE;
    }

    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(
            device,
            pipeline,
            nullptr);

        pipeline =
            VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(
            device,
            pipelineLayout,
            nullptr);

        pipelineLayout =
            VK_NULL_HANDLE;
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(
            device,
            descriptorSetLayout,
            nullptr);

        descriptorSetLayout =
            VK_NULL_HANDLE;
    }

    if (shaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(
            device,
            shaderModule,
            nullptr);

        shaderModule =
            VK_NULL_HANDLE;
    }
}