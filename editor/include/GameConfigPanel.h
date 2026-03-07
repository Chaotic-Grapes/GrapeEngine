/* Start Header *****************************************************************/
/*!
\file   GameConfigPanel.h
\author ChatGPT (implementation based on existing editor patterns)
\brief
Dockable ImGui panel for editing game ProjectSettings (ProjectSettings.json).

The panel exposes:
- Game metadata: Title, Version, StartupScene
- Window settings: Width, Height, Mode, VSync
- Physics: Gravity, TimeStep
- Audio: Master/Music/SFX volume, MuteWhenUnfocused
- Performance: TargetFPS, MaxFPS (only when VSync is disabled)

It reads/writes directly from/to the engine's ProjectSettings instance and
persists changes via Serialization::ConfigurationSerializer.
*/
/* End Header *******************************************************************/

#ifndef GAME_CONFIG_PANEL_H
#define GAME_CONFIG_PANEL_H

#include <string>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include "serialization/ConfigurationSerializer.h"
#include "core/ProjectPaths.h"

class GameConfigPanel {
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    // Opens (shows) the panel.
    void Open() { m_isOpen = true; }

    // Renders the panel for the given settings instance.
    void Render(ProjectSettings& settings);

private:
    ImFont* m_mainFont   = nullptr;
    ImFont* m_boldFont   = nullptr;
    ImFont* m_symbolsFont = nullptr;

    bool  m_isOpen   = false;
    bool  m_dirty    = false;
    double m_lastSavedTime = -100.0;

    void _renderGameSection(ProjectSettings& settings);
    void _renderWindowSection(ProjectSettings& settings);
    void _renderPhysicsSection(ProjectSettings& settings);
    void _renderAudioSection(ProjectSettings& settings);
    void _renderPerformanceSection(ProjectSettings& settings);
    void _renderFooter(ProjectSettings& settings);
};

#endif

