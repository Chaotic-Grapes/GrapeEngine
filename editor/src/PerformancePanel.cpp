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
#include "ecs/SystemManager.h"
#include "ecs/World.h"
#include "core/Logger.h"
#include <imgui.h>
#include <algorithm>
#include <deque>

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

void PerformancePanel::SetSystemManager(ECS::SystemManager* systemManager) {
    m_systemManager = systemManager;
}

void PerformancePanel::SetWorld(ECS::World* world) {
    m_world = world;
}

void PerformancePanel::Render(bool /*isPlaying*/) {
    if (!m_initialized) return;

    if (!ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Update cached data each frame (continuous monitoring)
    _updateCachedData();

    // Render header with FPS and frame time
    _renderHeader();

    ImGui::Separator();

    // Render overview statistics
    _renderOverviewStats();

    ImGui::Separator();

    // Render system usage table
    _renderSystemsTable();

    ImGui::End();
}

// -------------------------------------------------------------------------
// Private Rendering Methods
// -------------------------------------------------------------------------

void PerformancePanel::_renderHeader() {
    ImGui::PushFont(m_boldFont);
    ImGui::Text("Performance");
    ImGui::PopFont();
}

void PerformancePanel::_renderOverviewStats() {
    ImGui::PushFont(m_boldFont);
    ImGui::Text("Overview");
    ImGui::PopFont();

    // Display overview stats in columns
    ImGui::Columns(8, "OverviewColumns", false);
    
    ImGui::Text("FPS:");
    ImGui::NextColumn();
    ImGui::Text("%.1f", m_cachedFps);
    ImGui::NextColumn();
    
    ImGui::Text("Frame:");
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", m_cachedFrameMs);
    ImGui::NextColumn();

    ImGui::Text("Min:");
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", m_cachedMinFrameMs);
    ImGui::NextColumn();

    ImGui::Text("Max:");
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", m_cachedMaxFrameMs);
    ImGui::NextColumn();

    ImGui::Separator();

    ImGui::Text("Entities:");
    ImGui::NextColumn();
    ImGui::Text("%u", m_cachedEntityCount);
    ImGui::NextColumn();
    
    ImGui::Text("Components:");
    ImGui::NextColumn();
    ImGui::Text("%u", m_cachedComponentCount);
    ImGui::NextColumn();

    ImGui::Columns(1);
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

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableHeadersRow();

        // Render each system
        for (const auto &kv : m_cachedScopes) {
            const std::string &name = kv.first;
            const auto &data = kv.second;

            // Skip disabled systems from display
            // Even though we have [Disabled] tags, hiding them declutters the view
            if (m_systemManager) {
                if (!m_systemManager->IsSystemEnabled(name)) {
                    continue;
                }
            }

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

            // Column 0: System Name with type and status indicators
            ImGui::TableSetColumnIndex(0);
            
            // Get system info from cached data
            bool isScripted = data.isScripted;
            bool isEnabled = data.isEnabled;
            
            // Display system type indicator
            if (isScripted) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // Green for C#
                ImGui::Text("[C#]");
                ImGui::PopStyleColor();
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.6f, 0.8f, 1.0f)); // Blue for Native
                ImGui::Text("[Native]");
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();
            
            // Display status indicator (based on cached data, not current check)
            if (isEnabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 0.2f, 1.0f)); // Green for Enabled
                ImGui::Text("[Enabled]");
                ImGui::PopStyleColor();
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Gray for Disabled
                ImGui::Text("[Disabled]");
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", name.c_str());

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

            // Add padding after each system row
            ImGui::TableNextRow(ImGuiTableRowFlags_None);
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
            ImGui::Text("Engine");
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

            // Add padding after unattributed row
            ImGui::TableNextRow(ImGuiTableRowFlags_None);
        }

        ImGui::EndTable();
    }
}

// -------------------------------------------------------------------------
// Private Data Management
// -------------------------------------------------------------------------

void PerformancePanel::_updateCachedData() {
    m_hasCollectedData = true;

    // Cache live data continuously
    try {
        m_cachedFps = TimeSystem::Instance().GetFPS();
        m_cachedFrameMs = TimeSystem::Instance().GetFrameTimeMs();
        m_cachedTotalTime = TimeSystem::Instance().GetTotalScopeTimes();

        // Cache scope data and track min/max frame times
        const auto liveScopes = TimeSystem::Instance().GetAllScopeData();
        m_cachedScopes.clear();
        
        // Track frame time variation
        static std::deque<float> frameTimeHistory;
        frameTimeHistory.push_back(m_cachedFrameMs);
        if (frameTimeHistory.size() > 60) {  // Keep last 60 frames
            frameTimeHistory.pop_front();
        }
        
        if (!frameTimeHistory.empty()) {
            m_cachedMinFrameMs = *std::min_element(frameTimeHistory.begin(), frameTimeHistory.end());
            m_cachedMaxFrameMs = *std::max_element(frameTimeHistory.begin(), frameTimeHistory.end());
        }
        
        for (const auto &kv : liveScopes) {
            CachedScopeData cached;
            cached.AverageTimeMs = kv.second.AverageTimeMs;
            cached.MaxTimeMs = kv.second.MaxTimeMs;
            
            // Cache system info from SystemManager
            if (m_systemManager) {
                cached.isScripted = m_systemManager->IsScriptedSystem(kv.first);
                cached.isEnabled = m_systemManager->IsSystemEnabled(kv.first);
            }
            
            m_cachedScopes[kv.first] = cached;
        }

        // Get entity and component count from World if available
        if (m_world) {
            // Count entities by iterating through archetypes
            uint32_t totalEntities = 0;
            uint32_t totalComponents = 0;
            
            const auto archetypes = m_world->GetAllArchetypes();
            for (const auto* archetype : archetypes) {
                if (archetype) {
                    // Each archetype contains entities with a specific set of components
                    const uint32_t chunkCount = archetype->GetChunkCount();

                    for (uint32_t ci = 0; ci < chunkCount; ++ci) {
                        const auto* chunk = archetype->GetChunk(ci);

                        if (chunk) {
                            uint32_t entityCount = chunk->Count();
                            totalEntities += entityCount;

                            // Each entity in this archetype has the same number of components
                            uint32_t componentsPerEntity = static_cast<uint32_t>(archetype->GetComponents().size());
                            totalComponents += entityCount * componentsPerEntity;
                        }
                    }
                }
            }
            
            m_cachedEntityCount = totalEntities;
            m_cachedComponentCount = totalComponents;
        }
        else {
            m_cachedEntityCount = 0;
            m_cachedComponentCount = 0;
        }
    }
    catch (...) {
        // Keep existing cached values on error
    }
}
