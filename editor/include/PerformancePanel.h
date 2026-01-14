/* Start Header *****************************************************************/
/*!
\file   PerformancePanel.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th November 2025

\brief
Declares the in-editor performance monitoring panel used by the Level Editor.

\details
The PerformancePanel class provides a UI panel within the editor that displays
real-time performance metrics such as FPS, frame time, and profiling scope
data. It integrates with the engine's profiling system to gather timing data
and presents it in an organized manner for developers to monitor and analyze
the performance of their scenes during play mode.
*/
/* End Header *******************************************************************/

#ifndef PERFORMANCE_PANEL_H
#define PERFORMANCE_PANEL_H

#include <imgui.h>
#include <string>
#include <map>

class PerformancePanel {
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont);
    void Shutdown();

    // Render accepts whether the editor is currently playing; monitoring is paused when not playing
    void Render(bool isPlaying);
    
    // Reset the panel to show the paused message (call when loading a new scene)
    void ResetForNewScene();

private:
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    bool m_initialized = false;
    bool m_hasCollectedData = false;  // Track if we've collected data at least once
    
    // Cached data to freeze stats when not playing
    struct CachedScopeData {
        float AverageTimeMs = 0.0f;
        float MaxTimeMs = 0.0f;
    };

    float m_cachedFps = 0.0f;
    float m_cachedFrameMs = 0.0f;
    double m_cachedTotalTime = 0.0;
    
    std::map<std::string, CachedScopeData> m_cachedScopes;

    // Private rendering methods
    void _updateCachedData(bool isPlaying);
    void _renderHeader();
    void _renderSystemsTable();
};

#endif
