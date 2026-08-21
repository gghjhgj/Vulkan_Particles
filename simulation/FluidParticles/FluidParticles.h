#pragma once

#include "../../VK/VulkanContext.h"
#include "../../VK/VulkanBuffer.h"
#include "../../VK/VulkanTexture.h"

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <cstdint>

class ParticleSystem;
class FluidSystem;

class FluidParticles
{
public:
    FluidParticles() = default;
    ~FluidParticles() = default;

    void init(
        VulkanContext& context,
        ParticleSystem& particles,
        FluidSystem& fluid,
        const std::string& shaderPath,
        uint32_t pushConstantSize
    );

    void update(
        VulkanContext& context,
        ParticleSystem& particles,
        FluidSystem& fluid,
        const void* pushData,
        uint32_t pushConstantSize,
        VkSemaphore waitSemaphore = VK_NULL_HANDLE
    );

    void destroy(VkDevice device);

    VkSemaphore getComputeFinishedSemaphore() const
    {
        return computeFinishedSemaphore;
    }

private:
    VkShaderModule createShaderModule(VkDevice device, const std::string& path);

    uint32_t particleCount{0};

    VkSampler linearSampler{VK_NULL_HANDLE};

    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};

    VkDescriptorSet descriptorSets[2]{VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};

    VkFence computeFence{VK_NULL_HANDLE};
    VkSemaphore computeFinishedSemaphore{VK_NULL_HANDLE};
};