#pragma once

enum class ControlMode
{
    None,
    MouseParticles,
    MusicParticles,
    FluidMouse,
    FluidParticles,
    FluidParticlesMusic
};

class StartingScreen
{
public:
    ControlMode render();

private:
    ControlMode selectedMode = ControlMode::None;
};