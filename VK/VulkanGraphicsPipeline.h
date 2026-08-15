#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>

class VulkanContext;

class VulkanGraphicsPipeline
{
    public:

    VkPipeline handle = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    void init(
        const VulkanContext& context,
        const std::string& vertexShaderPath,
        const std::string& fragmentShaderPath,
        VkPrimitiveTopology topology,
        uint32_t pushConstantSize,
        VkFormat colorAttachmentFormat,
        const std::vector<VkDescriptorSetLayout>& descriptorLayouts
    );

    void destroy(VkDevice device);


    private:

    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
};