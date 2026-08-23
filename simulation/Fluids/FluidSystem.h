#pragma once

#include "FluidCell.h"
#include "../RGBA.h"
#include "../../Config/Config.h"
#include "../../VK/VulkanTexture.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <string>

class VulkanContext;

struct alignas(16) FluidPushConstants
{
    float mouseX{0.0f};
    float mouseY{0.0f};
    float prevMouseX{0.0f};
    float prevMouseY{0.0f};

    float dt{0.016f};
    float splatRadius{0.0f};
    float splatForce{0.0f};
    float velocityDissipation{0.0f};
    float densityDissipation{0.0f};
    float vorticity{0.0f};

    uint32_t renderWidth{0};  
    uint32_t renderHeight{0};
    uint32_t simWidth{0};
    uint32_t simHeight{0};
    uint32_t pressWidth{0};
    uint32_t pressHeight{0};
    uint32_t windowWidth{0};
    uint32_t windowHeight{0};

    uint32_t isMouseDown{0};
    uint32_t offsetFromLeft{Config::fluid.offsetFromLeft};
    uint32_t offsetFromRight{Config::fluid.offsetFromRight};
    uint32_t offsetFromUp{Config::fluid.offsetFromUp};
    uint32_t offsetFromDown{Config::fluid.offsetFromDown};

    float omega{Config::fluid.omega};
    uint32_t pressureSteps{Config::fluid.pressureSteps};
};

class FluidSystem
{
public:
    uint8_t getColorPingPong() const { return colorPingPong; }

    const VulkanTexture& getActiveColorTexture() const 
    {
        return (colorPingPong == 0) ? colorTextureA : colorTextureB;
    }

    const VulkanTexture& getActiveVelocityTexture() const 
    {
        return (velocityPingPong == 0) ? velocityTextureA : velocityTextureB; 
    }

    const VulkanTexture& getColorTextureA() const { return colorTextureA; }
    const VulkanTexture& getColorTextureB() const { return colorTextureB; }
    const VulkanTexture& getPressureTexture() const { return pressureTexture; }
    const VulkanTexture& getDivergenceTexture() const { return divergenceTexture; }
    const VulkanTexture& getVelocityTextureA() const { return velocityTextureA; }
    const VulkanTexture& getVelocityTextureB() const { return velocityTextureB; }

    void init(
        VulkanContext &context,
        uint32_t baseSimWidth,
        uint32_t baseSimHeight,
        const std::string &dummyPath = "",
        uint32_t pushConstantSize = sizeof(FluidPushConstants));

    void update(
        VulkanContext &context,
        const void *pushData,
        uint32_t pushConstantSize,
        VkSemaphore waitSemaphore = VK_NULL_HANDLE);

    void destroy(VkDevice device);

    uint32_t getCellCount() const { return simWidth * simHeight; }
    uint32_t getSimWidth() const { return simWidth; }
    uint32_t getSimHeight() const { return simHeight; }
    uint32_t getPressWidth() const { return pressWidth; }
    uint32_t getPressHeight() const { return pressHeight; }

    VkSemaphore getComputeFinishedSemaphore() const { return computeFinishedSemaphore; }

private:
    uint32_t pressureIterations = Config::fluid.pressureIterations;

    VkShaderModule createShaderModule(VkDevice device, const std::string &path);
    void createComputePipeline(
        VkDevice device,
        VkShaderModule module,
        VkPipelineLayout pipelineLayout,
        VkPipeline &outPipeline);

    uint32_t baseWidth{0};
    uint32_t baseHeight{0};

    uint32_t simWidth{0};
    uint32_t simHeight{0};
    uint32_t pressWidth{0};
    uint32_t pressHeight{0};

    VulkanTexture velocityTextureA;
    VulkanTexture velocityTextureB;
    VulkanTexture colorTextureA;
    VulkanTexture colorTextureB;
    VulkanTexture pressureTexture;
    VulkanTexture divergenceTexture;

    VkSampler linearSampler{VK_NULL_HANDLE};

    uint8_t colorPingPong = 0;
    uint8_t velocityPingPong = 0;

    VkDescriptorSetLayout splatDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout advectDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout jacobiDescriptorSetLayout{VK_NULL_HANDLE};

    VkPipelineLayout splatPipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout advectPipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout jacobiPipelineLayout{VK_NULL_HANDLE};

    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};

    VkDescriptorSet splatDescriptorSets[2]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDescriptorSet advectDescriptorSets[2]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDescriptorSet jacobiDescriptorSet{VK_NULL_HANDLE};

    VkPipeline pipelineSplat{VK_NULL_HANDLE};
    VkPipeline pipelineAdvect{VK_NULL_HANDLE};
    VkPipeline pipelineJacobi{VK_NULL_HANDLE};

    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};

    VkFence computeFence{VK_NULL_HANDLE};
    VkSemaphore computeFinishedSemaphore{VK_NULL_HANDLE};
};