#include "ParticleSystem.h"
#include "../VK/VulkanContext.h"

void ParticleSystem::init(
    VulkanContext& context,
    uint32_t count,
    uint32_t width,
    uint32_t height,
    const std::string& shaderPath,
    uint32_t pushConstantSize
)
{
    particles.resize(count);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> distVelocity(-2.0f, 2.0f);
    std::uniform_int_distribution<uint32_t> distColor(0, 255);

    int spawnX = width / 2;
    int spawnY = height / 2;

    for (uint32_t i = 0; i < count; ++i)
    {
        particles[i].x = static_cast<float>(spawnX);
        particles[i].y = static_cast<float>(spawnY);

        particles[i].prevX = static_cast<float>(spawnX);
        particles[i].prevY = static_cast<float>(spawnY);

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
        true
    );

    particleBuffer.upload(
        particles.data(),
        bufferSize
    );

    computePipeline.init(
        context,
        shaderPath,
        pushConstantSize,
        256,
        1,
        Config::particles.count
    );

    computePipeline.bindBuffers(
        context,
        {particleBuffer}
    );
}

void ParticleSystem::update(
    VulkanContext& context,
    const void* pushData,
    uint32_t pushConstantSize
)
{
    constexpr uint32_t WORKGROUP_SIZE = 256;

    uint32_t groupCount = (Config::particles.count + 255) / 256;

    computePipeline.dispatch(
        context,
        groupCount,
        1,
        1,
        pushData,
        pushConstantSize,
        particleBuffer.handle
    );
}

void ParticleSystem::destroy(
    VkDevice device
)
{
    computePipeline.destroy(device);
    particleBuffer.destroy(device);
}