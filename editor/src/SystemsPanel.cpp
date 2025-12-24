/* Start Header *****************************************************************/
/*!
\file   SystemsPanel.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implements the SystemsPanel class for displaying registered ECS systems.

Queries the SystemManager to collect information about all registered systems
and renders them in a table with metadata, grouping by execution phase.
Allows runtime toggling of system enable/disable states.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "SystemsPanel.h"
#include "ecs/SystemManager.h"
#include "ecs/World.h"
#include "ecs/ISystem.h"
#include "ecs/ISystemMetadataProvider.h"
#include "EditorStyle.h"
#include <imgui.h>
#include <algorithm>
#include <map>

// -------------------------------------------------------------------------
// Lifecycle Management
// -------------------------------------------------------------------------

void SystemsPanel::Initialize(ImFont* mainFont, ImFont* boldFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
}

void SystemsPanel::Shutdown() {
    m_systems.clear();
    m_selectedSystemName.clear();
}

void SystemsPanel::SetWorld(ECS::World* world) {
    m_world = world;
}

// -------------------------------------------------------------------------
// Panel Rendering
// -------------------------------------------------------------------------

void SystemsPanel::Render(ECS::SystemManager* systemManager) {
    if (!ImGui::Begin("Systems", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Update system list each frame (small overhead, ensures accurate display)
    // Cache the pointer for use in private rendering functions
    m_systemManager = systemManager;
    _updateSystemsList(systemManager);

    // Render header
    _renderHeader();

    ImGui::Separator();

    // Render main systems table
    _renderSystemsTable();

    ImGui::Separator();

    // Render statistics
    _renderStats();

    ImGui::End();
}

// -------------------------------------------------------------------------
// Private Rendering Methods
// -------------------------------------------------------------------------

void SystemsPanel::_renderHeader() {
    ImGui::PushFont(m_boldFont);
    ImGui::Text("Registered ECS Systems");
    ImGui::PopFont();

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    ImGui::TextDisabled("(Count: %zu)", m_systems.size());
}

void SystemsPanel::_renderSystemsTable() {
    // Group systems by execution phase for better organization
    std::map<std::string, std::vector<SystemInfo*>> groupedSystems;

    for (auto& system : m_systems) {
        groupedSystems[system.group].push_back(&system);
    }

    // Define execution group order (as they execute in the engine)
    static const std::vector<std::string> groupOrder = {
        "PreUpdate", "Update", "PostUpdate",
        "PrePhysics", "Physics", "PostPhysics",
        "PreRender", "Render", "PostRender"
    };

    // Render table with columns: Name, Group, Type, Enabled, Edit Mode
    if (ImGui::BeginTable("SystemsTable", 5,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {

        ImGui::TableSetupColumn("System Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Edit Mode", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        // Render systems in execution order, grouping by phase
        for (const auto& groupName : groupOrder) {
            auto it = groupedSystems.find(groupName);
            if (it == groupedSystems.end() || it->second.empty()) {
                continue; // Skip empty groups
            }

            const auto& systemsInGroup = it->second;
            
            // Group header row
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
            ImGui::Text("%s (%zu)", groupName.c_str(), systemsInGroup.size());
            ImGui::PopStyleColor();

            // Render each system in this group
            for (auto* pSystem : systemsInGroup) {
                SystemInfo& system = *pSystem;
                ImGui::TableNextRow();

                // Column 0: System Name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", system.name.c_str());

                // Column 1: Execution Group
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", system.group.c_str());

                // Column 2: Type (with C# vs C++ indicator)
                ImGui::TableSetColumnIndex(2);
                if (system.isScripted) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // Green for C#
                    ImGui::Text("[C#]");
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.6f, 0.8f, 1.0f)); // Blue for Native
                    ImGui::Text("[Native]");
                    ImGui::PopStyleColor();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", system.typeFullName.c_str());

                // Column 3: Enabled Toggle
                ImGui::TableSetColumnIndex(3);
                bool wasEnabled = system.isEnabled;
                ImGui::Checkbox(("##enabled_" + system.name).c_str(), &system.isEnabled);
                
                // Apply state change to SystemManager if toggled
                if (wasEnabled != system.isEnabled && m_systemManager != nullptr) {
                    m_systemManager->SetSystemEnabled(system.name, system.isEnabled);
                }

                // Column 4: Edit Mode Execution
                ImGui::TableSetColumnIndex(4);
                if (system.executeInEditMode) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); // Yellow
                    ImGui::Text("Yes");
                    ImGui::PopStyleColor();
                    
                    // Tooltip
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Runs when editor is paused");
                    }
                } else {
                    ImGui::TextDisabled("No");
                }
            }
        }

        ImGui::EndTable();
    }
}

void SystemsPanel::_renderStats() {
    size_t cppCount = 0;
    size_t csCount = 0;
    size_t enabledCount = 0;
    size_t editModeCount = 0;

    for (const auto& system : m_systems) {
        if (system.isScripted) {
            csCount++;
        } else {
            cppCount++;
        }
        if (system.isEnabled) {
            enabledCount++;
        }
        if (system.executeInEditMode) {
            editModeCount++;
        }
    }

    ImGui::Columns(4, "StatsColumns", false);
    
    ImGui::Text("Native Systems:");
    ImGui::NextColumn();
    ImGui::Text("%zu", cppCount);
    ImGui::NextColumn();
    
    ImGui::Text("C# Systems:");
    ImGui::NextColumn();
    ImGui::Text("%zu", csCount);
    ImGui::NextColumn();

    ImGui::Separator();
    ImGui::Separator();
    ImGui::Separator();
    ImGui::Separator();

    ImGui::Text("Enabled:");
    ImGui::NextColumn();
    ImGui::Text("%zu / %zu", enabledCount, m_systems.size());
    ImGui::NextColumn();
    
    ImGui::Text("Edit Mode:");
    ImGui::NextColumn();
    ImGui::Text("%zu", editModeCount);

    ImGui::Columns(1);
}

// -------------------------------------------------------------------------
// Private Data Management
// -------------------------------------------------------------------------

void SystemsPanel::_updateSystemsList(ECS::SystemManager* systemManager) {
    m_systems.clear();

    if (!systemManager) {
        return;
    }

    // Query all systems from the manager
    auto allSystems = systemManager->GetAllSystems();
    
    for (const auto& [name, group] : allSystems) {
        SystemInfo info;
        info.name = name;
        
        // Query additional metadata about the system
        ECS::ISystem* sys = systemManager->GetSystem(name);
            if (sys) {
                // Get type name from metadata
                info.typeFullName = sys->GetMetadata().GetName();

                // Check if it's a scripted system
                if (m_systemManager) {
                    info.isScripted = m_systemManager->IsScriptedSystem(name);
                }

                // Get enabled state
                info.isEnabled = sys->IsEnabled();

                // Get execution order from SystemMetadata
                info.sortOrder = sys->GetMetadata().GetExecutionOrder();

                // Get execution group name (convert enum to string)
                switch (group) {
                    case ECS::SystemGroup::PreUpdate:   info.group = "PreUpdate"; break;
                    case ECS::SystemGroup::Update:      info.group = "Update"; break;
                    case ECS::SystemGroup::PostUpdate:  info.group = "PostUpdate"; break;
                    case ECS::SystemGroup::PrePhysics:  info.group = "PrePhysics"; break;
                    case ECS::SystemGroup::Physics:     info.group = "Physics"; break;
                    case ECS::SystemGroup::PostPhysics: info.group = "PostPhysics"; break;
                    case ECS::SystemGroup::PreRender:   info.group = "PreRender"; break;
                    case ECS::SystemGroup::Render:      info.group = "Render"; break;
                    case ECS::SystemGroup::PostRender:  info.group = "PostRender"; break;
                    default:                            info.group = "Unknown"; break;
                }

                // Determine run mode using metadata priority helper
                auto runMode = ECS::GetSystemMetadataRunMode(sys);
                info.executeInEditMode = (runMode == ECS::SystemRunMode::Always) || (runMode == ECS::SystemRunMode::EditOnly);
            }

        m_systems.push_back(info);
    }

    // Sort systems by group order and sort order within group
    std::sort(m_systems.begin(), m_systems.end(),
        [](const SystemInfo& a, const SystemInfo& b) {
            // Define group order
            static const std::map<std::string, int> groupOrder{
                {"PreUpdate", 0}, {"Update", 1}, {"PostUpdate", 2},
                {"PrePhysics", 3}, {"Physics", 4}, {"PostPhysics", 5},
                {"PreRender", 6}, {"Render", 7}, {"PostRender", 8}
            };

            int aGroupOrder = groupOrder.count(a.group) ? groupOrder.at(a.group) : 999;
            int bGroupOrder = groupOrder.count(b.group) ? groupOrder.at(b.group) : 999;

            if (aGroupOrder != bGroupOrder) {
                return aGroupOrder < bGroupOrder;
            }
            
            // Within group, sort by sort order
            return a.sortOrder < b.sortOrder;
        }
    );
}
