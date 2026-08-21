#pragma once

#include "../../VK/VulkanComputePipeline.h"
#include <string>

class VulkanContext;
class ParticleSystem;
class FluidSystem;

class FluidParticles
{
public:
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

    VkSemaphore getComputeFinishedSemaphore() const;

private:
    VulkanComputePipeline computePipeline;
};