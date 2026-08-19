#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstdint>

class VulkanContext;
class VulkanBuffer;

class VulkanComputePipeline
{
public:
    void init(
        const VulkanContext& context,
        const std::string& shaderPath,
        uint32_t pushConstantSize,
        uint32_t workGroupSizeX,
        uint32_t bindingCount,
        uint32_t specializationConstant
    );

    void bindBuffers(
        const VulkanContext& context,
        const std::vector<VulkanBuffer>& buffers
    );

    void dispatch(
        const VulkanContext& context,
        uint32_t groupCountX,
        uint32_t groupCountY,
        uint32_t groupCountZ,
        const void* pushConstantData,
        uint32_t pushConstantSize,
        VkBuffer particleBuffer,
        uint32_t descriptorSetIndex = 0,
        VkSemaphore waitSemaphore = VK_NULL_HANDLE
    );

    void destroy(VkDevice device);

    VkSemaphore getFinishedSemaphore() const
    {
        return computeFinishedSemaphore;
    }

        void setupPingPongDescriptors(
    const VulkanContext& context,
    const VulkanBuffer& bufferA,
    const VulkanBuffer& bufferB
    );


private:
    std::vector<char> readFile(
        const std::string& filename);

    VkShaderModule createShaderModule(
        VkDevice device,
        const std::vector<char>& code);

private:
    VkShaderModule shaderModule =
        VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout =
        VK_NULL_HANDLE;

    VkPipelineLayout pipelineLayout =
        VK_NULL_HANDLE;

    VkPipeline pipeline =
        VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool =
        VK_NULL_HANDLE;

    std::vector<VkDescriptorSet> descriptorSets;

    VkCommandPool commandPool =
        VK_NULL_HANDLE;

    VkCommandBuffer commandBuffer =
        VK_NULL_HANDLE;

    VkFence computeFence =
        VK_NULL_HANDLE;

    VkSemaphore computeFinishedSemaphore =
        VK_NULL_HANDLE;
};