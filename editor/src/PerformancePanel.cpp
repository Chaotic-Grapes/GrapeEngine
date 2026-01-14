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
#include "services/TimeSystem.h"
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

    if (!ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Update cached data each frame
    _updateCachedData(isPlaying);

    // Show paused message only if we haven't collected any TimeSystem scope data yet
    if (!isPlaying && !m_hasCollectedData) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Scope monitoring paused — enter Play (Run) to collect per-system scope data");
        ImGui::End();
        return;
    }

    // Render header with FPS and frame time
    _renderHeader();

    ImGui::Separator();

    // Render system usage table
    _renderSystemsTable();

    ImGui::End();
}

// -------------------------------------------------------------------------
// Private Rendering Methods
// -------------------------------------------------------------------------

void PerformancePanel::_renderHeader() {
    ImGui::PushFont(m_mainFont);
    ImGui::Text("FPS: %.1f", m_cachedFps);
    ImGui::SameLine(150);
    ImGui::Text("Frame: %.2f ms", m_cachedFrameMs);
    ImGui::PopFont();
}

void PerformancePanel::_renderSystemsTable() {
    ImGui::PushFont(m_boldFont);
    ImGui::Text("System Usage");
    ImGui::PopFont();

    // Use cached total time. Ensure it's not smaller than the frame time
    // (some profilers report scope totals that are smaller due to sampling
    // or nested/overlapping scopes). Using at least the frame time prevents
    // usage percentages from exceeding 100% unexpectedly.
    double totalTime = m_cachedTotalTime;
    if (totalTime < 0.001)
        totalTime = m_cachedFrameMs; // Fallback to frame time if no scopes
    else
        totalTime = std::max(totalTime, static_cast<double>(m_cachedFrameMs));

    // Compute sum of scoped averages so we can show any "unattributed" time
    double sumScopeAvgMs = 0.0;
    for (const auto &kv : m_cachedScopes) {
        sumScopeAvgMs += static_cast<double>(kv.second.AverageTimeMs);
    }

    double unaccountedMs = totalTime - sumScopeAvgMs;
    if (unaccountedMs < 0.0)
        unaccountedMs = 0.0; // avoid negative due to rounding

    // Render table with columns: System Name, %, Avg (ms), Max (ms)
    if (ImGui::BeginTable("PerformanceTable", 4,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {

        ImGui::TableSetupColumn("System", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();

        // Render each system
        for (const auto &kv : m_cachedScopes) {
            const std::string &name = kv.first;
            const auto &data = kv.second;

            // Calculate usage percentage and clamp to [0, 100].
            float usagePercent = 0.0f;
            if (totalTime > 0.001) {
                double percent = (static_cast<double>(data.AverageTimeMs) / totalTime) * 100.0;
                if (percent < 0.0) percent = 0.0;
                if (percent > 100.0) percent = 100.0;
                usagePercent = static_cast<float>(percent);
            }

            // Determine bar color based on usage percentage
            ImVec4 barColor;
            if (usagePercent >= 80.0f) {
                barColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Red
            }
            else if (usagePercent >= 60.0f) {
                barColor = ImVec4(0.9f, 0.6f, 0.2f, 1.0f); // Orange
            }
            else if (usagePercent >= 40.0f) {
                barColor = ImVec4(0.9f, 0.9f, 0.2f, 1.0f); // Yellow
            }
            else {
                barColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
            }

            ImGui::TableNextRow();

            // Column 0: System Name
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", name.c_str());

            // Column 1: Usage % with color bar
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(usagePercent / 100.0f, ImVec2(-1.0f, 0.0f), "");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("%.1f%%", usagePercent);

            // Column 2: Average time
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", data.AverageTimeMs);

            // Column 3: Max time
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", data.MaxTimeMs);
        }

        // Show unattributed/render time
        if (unaccountedMs > 0.001) {
            double unaccountedPercent = (unaccountedMs / totalTime) * 100.0;
            if (unaccountedPercent < 0.0) unaccountedPercent = 0.0;
            if (unaccountedPercent > 100.0) unaccountedPercent = 100.0;

            ImGui::TableNextRow();

            // Column 0: System Name
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            ImGui::Text("Render");
            ImGui::PopStyleColor();

            // Column 1: Usage % with gray bar
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::ProgressBar(static_cast<float>(unaccountedPercent / 100.0), ImVec2(-1.0f, 0.0f), "");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("%.1f%%", static_cast<float>(unaccountedPercent));

            // Column 2: Average time
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", unaccountedMs);

            // Column 3: Max time (not tracked for render)
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("-");
        }

        ImGui::EndTable();
    }
}

// -------------------------------------------------------------------------
// Private Data Management
// -------------------------------------------------------------------------

void PerformancePanel::_updateCachedData(bool isPlaying) {
    if (isPlaying) {
        m_hasCollectedData = true;

        // Cache live data while playing
        try {
            m_cachedFps = TimeSystem::Instance().GetFPS();
            m_cachedFrameMs = TimeSystem::Instance().GetFrameTimeMs();
            m_cachedTotalTime = TimeSystem::Instance().GetTotalScopeTimes();

            // Cache scope data
            const auto liveScopes = TimeSystem::Instance().GetAllScopeData();
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
}
