#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <fstream>

class VulkanContext;
class VulkanBuffer;

class VulkanComputePipeline
{
    public:

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkShaderModule shaderModule = VK_NULL_HANDLE;

    void init(
        const VulkanContext& context, 
        const std::string& shaderPath, 
        uint32_t pushConstantSize, 
        uint32_t workGroupSizeX = 0, 
        uint32_t bindingCount = 1
    );

    void bindBuffers(
        const VulkanContext& context, 
        const std::vector<VulkanBuffer>& buffers
    );

    void dispatch(
        const VulkanContext& context, 
        uint32_t groupCountX, 
        uint32_t groupCountY = 1, 
        uint32_t groupCountZ = 1,
        const void* pushConstantData = nullptr,
        uint32_t pushConstantSize = 0
    );
    
    void destroy(VkDevice device);


    private:
    VkFence computeFence = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
};