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
        3,
        particles.getCount()
    );

    computePipeline.bindBuffers(context, { 
        particles.getBuffer(), 
        fluid.getVelocityBufferA(), 
        fluid.getColorBufferA() 
    }, 0);

    computePipeline.bindBuffers(context, { 
        particles.getBuffer(), 
        fluid.getVelocityBufferB(), 
        fluid.getColorBufferB() 
    }, 1);
}

void FluidParticles::update(
    VulkanContext& context,
    ParticleSystem& particles,
    FluidSystem& fluid,
    const void* pushData,
    uint32_t pushConstantSize,
    VkSemaphore waitSemaphore
)
{
    constexpr uint32_t WORKGROUP_SIZE = 256;
    uint32_t groupCount = (particles.getCount() + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

    uint32_t activeSetIndex = fluid.getColorPingPong();

    computePipeline.dispatch(
        context,
        groupCount,
        1,
        1,
        pushData,
        pushConstantSize,
        particles.getBuffer().handle,
        activeSetIndex,
        waitSemaphore
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