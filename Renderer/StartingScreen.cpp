#include "StartingScreen.h"
#include <imgui.h>

ControlMode StartingScreen::render()
{
    ImGuiIO& io = ImGui::GetIO();

    const float windowWidth = 380.0f;
    const float windowHeight = 420.0f;

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
        "Simulation Mode",
        nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings
    );

    ImGui::SetCursorPosY(20.0f);

    const char* title = "SELECT CONTROL MODE";
    float titleWidth = ImGui::CalcTextSize(title).x;

    ImGui::SetCursorPosX((windowWidth - titleWidth) * 0.5f);
    ImGui::TextUnformatted(title);

    ImGui::Spacing();
    ImGui::Spacing();

    constexpr float buttonWidth = 260.0f;
    constexpr float buttonHeight = 48.0f;

    ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
    if (ImGui::Button("MOUSE PARTICLES", ImVec2(buttonWidth, buttonHeight)))
    {
        selectedMode = ControlMode::MouseParticles;
    }

    ImGui::Spacing();

    ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
    if (ImGui::Button("MUSIC PARTICLES", ImVec2(buttonWidth, buttonHeight)))
    {
        selectedMode = ControlMode::MusicParticles;
    }

    ImGui::Spacing();

    ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
    if (ImGui::Button("FLUID MOUSE", ImVec2(buttonWidth, buttonHeight)))
    {
        selectedMode = ControlMode::FluidMouse;
    }

    ImGui::Spacing();

    ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
    if (ImGui::Button("FLUID PARTICLES", ImVec2(buttonWidth, buttonHeight)))
    {
        selectedMode = ControlMode::FluidParticles;
    }

    ImGui::Spacing();

    ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
    if (ImGui::Button("FLUID PARTICLES MUSIC", ImVec2(buttonWidth, buttonHeight)))
    {
        selectedMode = ControlMode::FluidParticlesMusic;
    }

    ImGui::End();

    return selectedMode;
}