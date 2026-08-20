#pragma once

#include "FluidCell.h"
#include "../RGBA.h"
#include "../../Config/Config.h"
#include "../../VK/VulkanBuffer.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <string>

class VulkanContext;

struct FluidPushConstants
{
    float mouseX;
    float mouseY;
    float prevMouseX;
    float prevMouseY;
    float dt;
    float splatRadius;
    float splatForce;
    float velocityDissipation;
    float densityDissipation;
    float vorticity;      
    uint32_t simWidth;
    uint32_t simHeight;
    uint32_t windowWidth;  
    uint32_t windowHeight;
    uint32_t isMouseDown;
};

class FluidSystem
{
public:
    void init(
        VulkanContext& context,
        uint32_t width,
        uint32_t height,
        const std::string& dummyPath = "",
        uint32_t pushConstantSize = sizeof(FluidPushConstants)
    );

    void update(
        VulkanContext& context,
        const void* pushData,
        uint32_t pushConstantSize,
        VkSemaphore waitSemaphore = VK_NULL_HANDLE
    );

    void destroy(
        VkDevice device
    );

    const VulkanBuffer& getActiveBuffer() const
    {
        return fluidBufferA;
    }

    uint32_t getCellCount() const
    {
        return width * height;
    }

    VkSemaphore getComputeFinishedSemaphore() const
    {
        return computeFinishedSemaphore;
    }

private:
    uint32_t pressureIterations = Config::fluid.pressureIterations;
    VkShaderModule createShaderModule(VkDevice device, const std::string& path);
    void createComputePipeline(VkDevice device, VkShaderModule module, VkPipeline& outPipeline);

    uint32_t width{0};
    uint32_t height{0};

    std::vector<FluidCell> cells;

    VulkanBuffer fluidBufferA;
    VulkanBuffer fluidBufferB;

    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptorSets;

    VkPipeline pipelineAdvect{VK_NULL_HANDLE};
    VkPipeline pipelineJacobi{VK_NULL_HANDLE};
    VkPipeline pipelineProject{VK_NULL_HANDLE};

    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkFence computeFence{VK_NULL_HANDLE};
    VkSemaphore computeFinishedSemaphore{VK_NULL_HANDLE};
};