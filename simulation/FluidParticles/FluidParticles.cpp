#include "FluidParticles.h"
#include "../Particles/ParticleSystem.h"
#include "../Fluids/FluidSystem.h"
#include "../../VK/VulkanContext.h"
#include "../../Config/Config.h"

void FluidParticles::init(
    VulkanContext& context,
    ParticleSystem& particles,
    FluidSystem& fluid,
    const std::string& shaderPath,
    uint32_t pushConstantSize
)
{
    computePipeline.init(
        context,
        shaderPath,
        pushConstantSize,
        256, 
        2, 
        particles.getCount()
    );

    computePipeline.bindBuffers(
        context,
        { particles.getBuffer(), fluid.getActiveBuffer() }
    );
}

void FluidParticles::update(
    VulkanContext& context,
    ParticleSystem& particles,
    FluidSystem& fluid,
    const void* pushData,
    uint32_t pushConstantSize
)
{
    constexpr uint32_t WORKGROUP_SIZE = 256;
    uint32_t groupCount = (particles.getCount() + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

    computePipeline.dispatch(
        context,
        groupCount,
        1,
        1,
        pushData,
        pushConstantSize,
        particles.getBuffer().handle,
        0,
        fluid.getComputeFinishedSemaphore()
    );
}

void FluidParticles::destroy(VkDevice device)
{
    computePipeline.destroy(device);
}

VkSemaphore FluidParticles::getComputeFinishedSemaphore() const
{
    return computePipeline.getFinishedSemaphore();
}