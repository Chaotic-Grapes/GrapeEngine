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
#include <algorithm>

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

    // Use cached total time. Ensure it's not smaller than the frame time
    // (some profilers report scope totals that are smaller due to sampling
    // or nested/overlapping scopes). Using at least the frame time prevents
    // usage percentages from exceeding 100% unexpectedly.
    double totalTime = m_cachedTotalTime;
    if (totalTime < 0.001)
        totalTime = frameMs; // Fallback to frame time if no scopes
    else
        totalTime = std::max(totalTime, static_cast<double>(frameMs));

    // List each system with cached formatting
    // Compute sum of scoped averages so we can show any "unattributed" time
    double sumScopeAvgMs = 0.0;
    for (const auto &kv : m_cachedScopes) {
        sumScopeAvgMs += static_cast<double>(kv.second.AverageTimeMs);
    }

    double unaccountedMs = totalTime - sumScopeAvgMs;
    if (unaccountedMs < 0.0)
        unaccountedMs = 0.0; // avoid negative due to rounding

    // Show a short summary of attributed vs unattributed time
    {
        double attributedPercent = 0.0;
        double unaccountedPercent = 0.0;
        if (totalTime > 0.001) {
            attributedPercent = (sumScopeAvgMs / totalTime) * 100.0;
            unaccountedPercent = (unaccountedMs / totalTime) * 100.0;
            if (attributedPercent < 0.0) attributedPercent = 0.0;
            if (attributedPercent > 100.0) attributedPercent = 100.0;
            if (unaccountedPercent < 0.0) unaccountedPercent = 0.0;
            if (unaccountedPercent > 100.0) unaccountedPercent = 100.0;
        }

        ImGui::PushFont(m_boldFont);
        ImGui::Text("Breakdown: Attributed %.2f ms (%.1f%%)  Unattributed %.2f ms (%.1f%%)",
                    sumScopeAvgMs, attributedPercent, unaccountedMs, unaccountedPercent);
        ImGui::PopFont();
        ImGui::Spacing();
    }

    for (const auto &kv : m_cachedScopes) {
        const std::string &name = kv.first;
        // TODO: Fix Render system to follow usual pattern, then remove this line
        if (name == "Render") continue; // Skip, because it's not following the usual system pattern for now
        const auto &data = kv.second;
        
        // Calculate usage percentage and clamp to [0, 100].
        float usagePercent = 0.0f;
        if (totalTime > 0.001) {
            double percent = (static_cast<double>(data.AverageTimeMs) / totalTime) * 100.0;
            if (percent < 0.0) percent = 0.0;
            if (percent > 100.0) percent = 100.0;
            usagePercent = static_cast<float>(percent);
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

    // If there's unaccounted time, show it as "Other / Unattributed"
    // Usually, this is renderer
    if (unaccountedMs > 0.001) {
        double unaccountedPercent = (unaccountedMs / totalTime) * 100.0;
        if (unaccountedPercent < 0.0) unaccountedPercent = 0.0;
        if (unaccountedPercent > 100.0) unaccountedPercent = 100.0;

        ImGui::PushFont(m_boldFont);
        //ImGui::Text("Others / Unattributed (e.g., underlying system calls)");
        ImGui::Text("Render");
        ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::ProgressBar(static_cast<float>(unaccountedPercent / 100.0), ImVec2(-1.0f, 0.0f));
        ImGui::PopStyleColor();
        ImGui::Text("  %.1f%% | %.2f ms", static_cast<float>(unaccountedPercent), unaccountedMs);
        ImGui::Spacing();
    }

    ImGui::End();
    ImGui::PopFont();
}
