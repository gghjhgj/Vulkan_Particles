#pragma once

#include "RGB.h"
#include "Particle.h"
#include "../VK/VulkanBuffer.h"
#include "../VK/VulkanComputePipeline.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <random>
#include <cstdint>
#include <string>

class VulkanContext;

struct ComputePush
{
    uint32_t particleCount;
    float mouseX;
    float mouseY;
};

struct MusicPush
{
    uint32_t particleCount;
};

class ParticleSystem
{
public:
    void init(
        VulkanContext& context,
        uint32_t count,
        uint32_t width,
        uint32_t height,
        const std::string& shaderPath,
        uint32_t pushConstantSize
    );

    void update(
        VulkanContext& context,
        const void* pushData,
        uint32_t pushConstantSize
    );

    void destroy(
        VkDevice device
    );

    const VulkanBuffer& getBuffer() const
    {
        return particleBuffer;
    }

    uint32_t getCount() const
    {
        return static_cast<uint32_t>(
            particles.size()
        );
    }

    VkSemaphore getComputeFinishedSemaphore() const
    {
        return computePipeline.getFinishedSemaphore();
    }

private:
    std::vector<Particle> particles;

    VulkanBuffer particleBuffer;
    VulkanComputePipeline computePipeline;
};