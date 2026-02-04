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

// Forward declarations
namespace ECS {
    class SystemManager;
    class World;
}

class PerformancePanel {
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont);
    void Shutdown();

    // Render accepts whether the editor is currently playing; monitoring is paused when not playing
    void Render(bool isPlaying);

    /**
     * @brief Set the World for gathering entity/component counts.
     * @param world Pointer to the ECS World (may be nullptr)
     */
    void SetWorld(ECS::World* world);

    // Reset the panel to show the paused message (call when loading a new scene)
    void ResetForNewScene();

    /**
     * @brief Set the SystemManager for filtering disabled systems.
     * @param systemManager Pointer to SystemManager (may be nullptr)
     */
    void SetSystemManager(ECS::SystemManager* systemManager);

private:
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    bool m_initialized = false;
    bool m_hasCollectedData = false;  // Track if we've collected data at least once
    ECS::SystemManager* m_systemManager = nullptr;
    ECS::World* m_world = nullptr;

    // Cached data to freeze stats when not playing
    struct CachedScopeData {
        float AverageTimeMs = 0.0f;
        float MaxTimeMs = 0.0f;
        bool isEnabled = true;
        bool isScripted = false;
    };

    float m_cachedFps = 0.0f;
    float m_cachedFrameMs = 0.0f;
    float m_cachedMinFrameMs = 0.0f;
    float m_cachedMaxFrameMs = 0.0f;
    double m_cachedTotalTime = 0.0;

    uint32_t m_cachedEntityCount = 0;
    uint32_t m_cachedComponentCount = 0;

    std::map<std::string, CachedScopeData> m_cachedScopes;

    // Memory allocator Testing
    bool m_useCustomAllocator = true;
    double m_lastTestTime = -1.0;  // Just a sentinel value (to indicate no test run yet)

    // Memory stats validation (runs once)
    bool m_statsValidated = false;

    // Private rendering methods
    void _updateCachedData();
    void _renderHeader();
    void _renderOverviewStats();
    void _renderSystemsTable();
    void _renderMemoryStats();
    void _renderMemoryBenchmark();
    bool _validateMemoryStats();
};

#endif