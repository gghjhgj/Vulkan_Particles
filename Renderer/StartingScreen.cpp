#include "StartingScreen.h"

#include <imgui.h>

ControlMode StartingScreen::render()
{
    ImGuiIO& io = ImGui::GetIO();

    const float windowWidth = 360.0f;
    const float windowHeight = 280.0f;

    ImGui::SetNextWindowPos(
        ImVec2(
            (io.DisplaySize.x - windowWidth) * 0.5f,
            (io.DisplaySize.y - windowHeight) * 0.5f
        ),
        ImGuiCond_Always
    );

    ImGui::SetNextWindowSize(
        ImVec2(windowWidth, windowHeight),
        ImGuiCond_Always
    );

    ImGui::Begin(
        "Particle Simulation",
        nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings
    );

    ImGui::SetCursorPosY(35.0f);

    const char* title = "SELECT CONTROL MODE";

    float titleWidth =
        ImGui::CalcTextSize(title).x;

    ImGui::SetCursorPosX(
        (windowWidth - titleWidth) * 0.5f
    );

    ImGui::TextUnformatted(title);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    constexpr float buttonWidth = 220.0f;
    constexpr float buttonHeight = 55.0f;

    ImGui::SetCursorPosX(
        (windowWidth - buttonWidth) * 0.5f
    );

    if (ImGui::Button(
        "MOUSE",
        ImVec2(buttonWidth, buttonHeight)
    ))
    {
        selectedMode = ControlMode::Mouse;
    }

    ImGui::Spacing();

    ImGui::SetCursorPosX(
        (windowWidth - buttonWidth) * 0.5f
    );

    if (ImGui::Button(
        "MUSIC",
        ImVec2(buttonWidth, buttonHeight)
    ))
    {
        selectedMode = ControlMode::Music;
    }

    ImGui::End();

    return selectedMode;
}