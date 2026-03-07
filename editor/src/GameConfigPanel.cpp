/* Start Header *****************************************************************/
/*!
\file   GameConfigPanel.cpp
\brief  Implementation of the dockable Game Configuration panel.
*/
/* End Header *******************************************************************/

#include "GameConfigPanel.h"

#include "EditorStyle.h"
#include "EditorIcons.h"
#include <imgui.h>

void GameConfigPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
}

void GameConfigPanel::Render(ProjectSettings& settings) {
    if (!m_isOpen) {
        return;
    }

    ImGui::PushFont(m_mainFont);
    if (!ImGui::Begin("Game Configuration", &m_isOpen)) {
        ImGui::End();
        ImGui::PopFont();
        return;
    }

    ImGui::TextDisabled("ProjectSettings.json");
    ImGui::Separator();

    _renderGameSection(settings);
    ImGui::Separator();
    _renderWindowSection(settings);
    ImGui::Separator();
    _renderPhysicsSection(settings);
    ImGui::Separator();
    _renderAudioSection(settings);
    ImGui::Separator();
    _renderPerformanceSection(settings);

    ImGui::Separator();
    _renderFooter(settings);

    ImGui::End();
    ImGui::PopFont();
}

void GameConfigPanel::_renderGameSection(ProjectSettings& settings) {
    if (ImGui::CollapsingHeader("Game", ImGuiTreeNodeFlags_DefaultOpen)) {
        char titleBuf[256];
        char versionBuf[64];
        char sceneBuf[512];

        strncpy_s(titleBuf, settings.Title.c_str(), sizeof(titleBuf) - 1);
        strncpy_s(versionBuf, settings.Version.c_str(), sizeof(versionBuf) - 1);
        strncpy_s(sceneBuf, settings.StartupScene.c_str(), sizeof(sceneBuf) - 1);

        if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
            settings.Title = titleBuf;
            m_dirty = true;
        }
        if (ImGui::InputText("Version", versionBuf, sizeof(versionBuf))) {
            settings.Version = versionBuf;
            m_dirty = true;
        }
        if (ImGui::InputText("Startup Scene", sceneBuf, sizeof(sceneBuf))) {
            settings.StartupScene = sceneBuf;
            m_dirty = true;
        }
    }
}

void GameConfigPanel::_renderWindowSection(ProjectSettings& settings) {
    if (ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen)) {
        int width = settings.WindowSettings.Width;
        int height = settings.WindowSettings.Height;
        if (ImGui::InputInt("Width", &width)) {
            settings.WindowSettings.Width = std::max(1, width);
            m_dirty = true;
        }
        if (ImGui::InputInt("Height", &height)) {
            settings.WindowSettings.Height = std::max(1, height);
            m_dirty = true;
        }

        // Window mode combo
        const char* modes[] = { "Windowed", "Fullscreen", "Borderless" };
        std::string modeValue = settings.WindowSettings.Mode;
        int modeIndex = 0;
        if (modeValue == "Fullscreen") {
            modeIndex = 1;
        } else if (modeValue == "Borderless") {
            modeIndex = 2;
        }
        if (ImGui::Combo("Mode", &modeIndex, modes, 3)) {
            if (modeIndex == 0) settings.WindowSettings.Mode = "Windowed";
            else if (modeIndex == 1) settings.WindowSettings.Mode = "Fullscreen";
            else settings.WindowSettings.Mode = "Borderless";
            settings.WindowSettings.Fullscreen = (settings.WindowSettings.Mode == "Fullscreen");
            m_dirty = true;
        }

        bool vsync = settings.WindowSettings.VSync;
        if (ImGui::Checkbox("VSync", &vsync)) {
            settings.WindowSettings.VSync = vsync;
            m_dirty = true;
        }
    }
}

void GameConfigPanel::_renderPhysicsSection(ProjectSettings& settings) {
    if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        float gravity = settings.Physics.Gravity;
        if (ImGui::InputFloat("Gravity", &gravity)) {
            settings.Physics.Gravity = gravity;
            m_dirty = true;
        }
        float timeStep = settings.Physics.TimeStep;
        if (ImGui::InputFloat("Time Step", &timeStep)) {
            settings.Physics.TimeStep = timeStep;
            m_dirty = true;
        }
    }
}

void GameConfigPanel::_renderAudioSection(ProjectSettings& settings) {
    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
        float master = settings.MasterVolume;
        float music  = settings.MusicVolume;
        float sfx    = settings.SFXVolume;

        if (ImGui::SliderFloat("Master Volume", &master, 0.0f, 1.0f)) {
            settings.MasterVolume = master;
            m_dirty = true;
        }
        if (ImGui::SliderFloat("Music Volume", &music, 0.0f, 1.0f)) {
            settings.MusicVolume = music;
            m_dirty = true;
        }
        if (ImGui::SliderFloat("SFX Volume", &sfx, 0.0f, 1.0f)) {
            settings.SFXVolume = sfx;
            m_dirty = true;
        }

        bool mute = settings.MuteWhenUnfocused;
        if (ImGui::Checkbox("Mute when unfocused", &mute)) {
            settings.MuteWhenUnfocused = mute;
            m_dirty = true;
        }
    }
}

void GameConfigPanel::_renderPerformanceSection(ProjectSettings& settings) {
    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        int targetFps = settings.TargetFPS;
        if (ImGui::InputInt("Target FPS", &targetFps)) {
            settings.TargetFPS = std::max(1, targetFps);
            m_dirty = true;
        }

        // Mirror VSync toggle here for convenience.
        bool vsync = settings.WindowSettings.VSync;
        if (ImGui::Checkbox("VSync (performance)", &vsync)) {
            settings.WindowSettings.VSync = vsync;
            m_dirty = true;
        }

        if (!settings.WindowSettings.VSync) {
            int maxFps = settings.MaxFPS;
            if (ImGui::InputInt("Max FPS", &maxFps)) {
                settings.MaxFPS = std::max(1, maxFps);
                m_dirty = true;
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::InputInt("Max FPS", &settings.MaxFPS);
            ImGui::EndDisabled();
        }
    }
}

void GameConfigPanel::_renderFooter(ProjectSettings& settings) {
    const float footerHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::Dummy(ImVec2(0.0f, footerHeight * 0.25f));

    ImGui::PushStyleColor(ImGuiCol_Button,   EditorStyle::PrimaryButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::PrimaryButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  EditorStyle::PrimaryButtonActive);

    if (ImGui::Button("Save", ImVec2(100.0f, 0.0f))) {
        const std::string settingsPath = Engine::ProjectPaths::GetSettingsPath();
        if (Serialization::ConfigurationSerializer::SaveProjectSettings(settingsPath, settings)) {
            m_dirty = false;
            m_lastSavedTime = ImGui::GetTime();
        }
    }

    ImGui::PopStyleColor(3);

    if (m_dirty) {
        ImGui::SameLine();
        ImGui::TextColored(EditorStyle::WarningText, "* Unsaved changes");
    } else if (ImGui::GetTime() - m_lastSavedTime < 2.0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "Saved");
    }
}

