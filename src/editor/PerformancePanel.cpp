/* Start Header *****************************************************************/
/*!
\file   PerformancePanel.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th November 2025

\brief
Implements the in-editor performance monitoring panel used by the Level Editor.

Displays realtime performance metrics while the editor is in Play mode:
- FPS and frame time (from the engine Profiler)
- System usage breakdown per registered profiling scope

Monitoring is paused when the editor is not in Play state to avoid
polling system counters while the game is not running.
*/
/* End Header *******************************************************************/

#include "PerformancePanel.h"
#include "core/Profiler.h"
#include "core/Logger.h"
#include <imgui.h>

void PerformancePanel::Initialize(ImFont* mainFont, ImFont* boldFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_initialized = true;
}

void PerformancePanel::Shutdown() {
    m_initialized = false;
    m_hasCollectedData = false;
}

void PerformancePanel::ResetForNewScene() {
    m_hasCollectedData = false;
}

void PerformancePanel::Render(bool isPlaying) {
    if (!m_initialized) return;

    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Performance");

    // Show paused message only if we haven't collected any data yet
    if (!isPlaying && !m_hasCollectedData) {
        ImGui::TextColored(ImVec4(1,1,0,1), "Monitoring paused — enter Play (Run) to collect data");
        ImGui::End();
        ImGui::PopFont();
        return;
    }
    
    // Mark that we've collected data once play mode is active
    if (isPlaying) {
        m_hasCollectedData = true;
        
        // Cache live data while playing
        try {
            m_cachedFps = Profiler::GetFPS();
            m_cachedFrameMs = Profiler::GetFrameTimeMs();
            m_cachedTotalTime = Profiler::GetTotalScopeTimes();
            
            // Cache scope data
            const auto &liveScopes = Profiler::GetAllScopeData();
            m_cachedScopes.clear();
            for (const auto &kv : liveScopes) {
                CachedScopeData cached;
                cached.AverageTimeMs = kv.second.AverageTimeMs;
                cached.MaxTimeMs = kv.second.MaxTimeMs;
                m_cachedScopes[kv.first] = cached;
            }
        }
        catch (...) {
            // Keep existing cached values on error
        }
    }

    // Use cached values (frozen when stopped, live when playing)
    float fps = m_cachedFps;
    float frameMs = m_cachedFrameMs;

    ImGui::Text("FPS: %.1f", fps);
    ImGui::SameLine(150);
    ImGui::Text("Frame: %.2f ms", frameMs);

    ImGui::Separator();
    ImGui::PushFont(m_boldFont);
    ImGui::Text("System Usage");
    ImGui::PopFont();
    ImGui::Spacing();

    // Use cached total time
    double totalTime = m_cachedTotalTime;
    if (totalTime < 0.001)
        totalTime = frameMs; // Fallback to frame time if no scopes

    // List each system with cached formatting
    for (const auto &kv : m_cachedScopes) {
        const std::string &name = kv.first;
        const auto &data = kv.second;
        
        // Calculate usage percentage
        float usagePercent = 0.0f;
        if (totalTime > 0.001f) {
            usagePercent = (data.AverageTimeMs / static_cast<float>(totalTime)) * 100.0f;
        }
        
        // System name header
        ImGui::PushFont(m_boldFont);
        ImGui::Text("%s", name.c_str());
        ImGui::PopFont();
        
        // Determine bar color based on usage percentage
        ImVec4 barColor;
        if (usagePercent >= 80.0f) {
            // Red for very high usage (>= 80%)
            barColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
        }
        else if (usagePercent >= 60.0f) {
            // Orange for high usage (60-79%)
            barColor = ImVec4(0.9f, 0.6f, 0.2f, 1.0f);
        }
        else if (usagePercent >= 40.0f) {
            // Yellow for moderate usage (40-59%)
            barColor = ImVec4(0.9f, 0.9f, 0.2f, 1.0f);
        }
        else {
            // Green for low usage (< 40%)
            barColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
        }
        
        // Bar graph showing usage
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
        ImGui::ProgressBar(usagePercent / 100.0f, ImVec2(-1.0f, 0.0f));
        ImGui::PopStyleColor();
        
        // Detailed stats on one line
        ImGui::Text("  %.1f%% | Avg: %.2f ms | Max: %.2f ms", usagePercent, data.AverageTimeMs, data.MaxTimeMs);
        
        ImGui::Spacing();
    }

    ImGui::End();
    ImGui::PopFont();
}
