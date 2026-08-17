#pragma once

enum class ControlMode
{
    None,
    Mouse,
    Music
};

class StartingScreen
{
public:
    ControlMode render();

private:
    ControlMode selectedMode =
        ControlMode::None;
};