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

    uint32_t phase;
};

class FluidSystem
{
public:
    std::vector<VelocityCell> velocityCells;
    std::vector<ColorCell> colorCells;
    std::vector<float> pressures;
    std::vector<float> divergences;

    uint8_t getColorPingPong() const 
    { 
        return colorPingPong; 
    }

    const VulkanBuffer& getActiveBuffer() const
    {
        return (colorPingPong == 0) ? colorBufferA : colorBufferB;
    }

    const VulkanBuffer& getActiveColorBuffer() const 
    {
        return (colorPingPong == 0) ? colorBufferA : colorBufferB;
    }

    const VulkanBuffer& getActiveVelocityBuffer() const 
    {
        return (velocityPingPong == 0) ? velocityBufferA : velocityBufferB; 
    }

    const VulkanBuffer& getVelocityBufferA() const { return velocityBufferA; }
    const VulkanBuffer& getVelocityBufferB() const { return velocityBufferB; }

    const VulkanBuffer& getColorBufferA() const { return colorBufferA; }
    const VulkanBuffer& getColorBufferB() const { return colorBufferB; }
    const VulkanBuffer& getPressureBuffer() const { return pressureBuffer; }

    void init(
        VulkanContext &context,
        uint32_t width,
        uint32_t height,
        const std::string &dummyPath = "",
        uint32_t pushConstantSize = sizeof(FluidPushConstants));

    void update(
        VulkanContext &context,
        const void *pushData,
        uint32_t pushConstantSize,
        VkSemaphore waitSemaphore = VK_NULL_HANDLE);

    void destroy(
        VkDevice device);

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

    VkShaderModule createShaderModule(
        VkDevice device,
        const std::string &path);

    void createComputePipeline(
        VkDevice device,
        VkShaderModule module,
        VkPipelineLayout pipelineLayout,
        VkPipeline &outPipeline);

    uint32_t width{0};
    uint32_t height{0};

    VulkanBuffer velocityBufferA;
    VulkanBuffer velocityBufferB;

    VulkanBuffer colorBufferA;
    VulkanBuffer colorBufferB;

    VulkanBuffer pressureBuffer;

    VulkanBuffer divergenceBuffer;

    uint8_t colorPingPong = 0;
    uint8_t velocityPingPong = 0;

    VkDescriptorSetLayout advectDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout jacobiDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout projectDescriptorSetLayout{VK_NULL_HANDLE};

    VkPipelineLayout advectPipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout jacobiPipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout projectPipelineLayout{VK_NULL_HANDLE};

    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};

    VkDescriptorSet advectDescriptorSets[2]{
        VK_NULL_HANDLE,
        VK_NULL_HANDLE};

    VkDescriptorSet jacobiDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet projectDescriptorSet{VK_NULL_HANDLE};

    VkPipeline pipelineAdvect{VK_NULL_HANDLE};
    VkPipeline pipelineJacobi{VK_NULL_HANDLE};
    VkPipeline pipelineProject{VK_NULL_HANDLE};

    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};

    VkFence computeFence{VK_NULL_HANDLE};
    VkSemaphore computeFinishedSemaphore{VK_NULL_HANDLE};
};