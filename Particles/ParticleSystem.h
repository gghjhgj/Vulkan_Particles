#pragma once

#include "Particle.h"
#include "../VK/VulkanBuffer.h"
#include "../VK/VulkanComputePipeline.h"

#include <vector>
#include <cstdint>
#include <random>

class VulkanContext;

class ParticleSystem
{
public:
    void init(
        VulkanContext& context,
        uint32_t count,
        uint32_t width,
        uint32_t height
    );

    void update(
        VulkanContext& context,
        float mouseX,
        float mouseY
    );

    void destroy(VkDevice device);

    const std::vector<Particle>& getParticles() const
    {
        return particles;
    }

    uint32_t getCount() const
    {
        return static_cast<uint32_t>(particles.size());
    }

    const VulkanBuffer& getBuffer() const
    {
        return particleBuffer;
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