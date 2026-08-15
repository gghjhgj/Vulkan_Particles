#include "ParticleSystem.h"
#include "../VK/VulkanContext.h"

struct ComputePush
{
    uint32_t particleCount;
    float mouseX;
    float mouseY;
};

void ParticleSystem::init(
    VulkanContext &context,
    uint32_t count,
    uint32_t width,
    uint32_t height)
{
    particles.resize(count);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> distVelocity(-2.0f, 2.0f);
    std::uniform_int_distribution<uint32_t> distColor(0, 255);

    int spawnX = width/2;
    int spawnY = height/2;
    for (uint32_t i = 0; i < count; ++i)
    {
        particles[i].x = spawnX;
        particles[i].y = spawnY;
        particles[i].vx = distVelocity(gen);
        particles[i].vy = distVelocity(gen);

        particles[i].color = RGB(
            static_cast<uint8_t>(distColor(gen)),
            static_cast<uint8_t>(distColor(gen)),
            static_cast<uint8_t>(distColor(gen))
        );
    }

    VkDeviceSize bufferSize =
        particles.size() * sizeof(Particle);

    particleBuffer.initStorage(
        context,
        bufferSize,
        true);

    particleBuffer.upload(
        particles.data(),
        bufferSize);

    computePipeline.init(
        context,
        "shaders/particle.comp.spv",
        sizeof(ComputePush),
        256,
        1);

    computePipeline.bindBuffers(
        context,
        {particleBuffer});
}

void ParticleSystem::update(
    VulkanContext &context,
    float mouseX,
    float mouseY)
{
    constexpr uint32_t WORKGROUP_SIZE = 256;

    uint32_t particleCount =
        static_cast<uint32_t>(particles.size());

    uint32_t groupCount =
        (particleCount + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

    ComputePush pushData{};
    pushData.particleCount = particleCount;
    pushData.mouseX = mouseX;
    pushData.mouseY = mouseY;

    computePipeline.dispatch(
        context,
        groupCount,
        1,
        1,
        &pushData,
        sizeof(ComputePush));
}

void ParticleSystem::destroy(VkDevice device)
{
    computePipeline.destroy(device);
    particleBuffer.destroy(device);
}