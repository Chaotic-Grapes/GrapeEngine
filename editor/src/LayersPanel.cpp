/* Start Header *****************************************************************/
/*!
\file    LayersPanel.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implements LayersPanel which allows viewing and editing scene layers.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#include "LayersPanel.h"
#include <imgui.h>
#include "core/Logger.h"
#include "ecs/Components.h"
#include "EditorFileMenu.h"
#include "HierarchyPanel.h"
#include "EditorEntityActions.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"

void LayersPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
}

void LayersPanel::SetScene(Scenes::Scene* scene) {
    // Unsubscribe previous subscription if any
    if (m_sceneModifiedSubscription.IsValid()) {
        Messaging::MessageSystem::Unsubscribe<Messaging::SceneModified>(m_sceneModifiedSubscription);
    }

    m_scene = scene;

    // Subscribe to scene modification events so the panel can refresh
    if (m_scene) {
        m_sceneModifiedSubscription = Messaging::MessageSystem::Subscribe<Messaging::SceneModified>(
            [this](const Messaging::SceneModified& e) {
                (void)e;
                m_needsRefresh = true;
            }
        );
    }
}

LayersPanel::~LayersPanel() {
    if (m_sceneModifiedSubscription.IsValid()) {
        Messaging::MessageSystem::Unsubscribe<Messaging::SceneModified>(m_sceneModifiedSubscription);
    }
}

void LayersPanel::Render() {
    if (m_mainFont) ImGui::PushFont(m_mainFont);
    ImGui::Begin("Layers");

    if (!m_scene) {
        ImGui::TextDisabled("No scene attached");
        if (m_mainFont) ImGui::PopFont();
        ImGui::End();
        return;
    }

    auto& lm = m_scene->GetLayers();
    auto layers = lm.ListLayers();

    // Header: new layer
    ImGui::InputText("##NewLayerName", m_newLayerName, sizeof(m_newLayerName));
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        std::string nm(m_newLayerName);
        if (!nm.empty()) {
            uint16_t id = lm.CreateLayer(nm);
            (void)id;
            if (m_fileMenu) m_fileMenu->MarkSceneDirty();
        }
    }

    ImGui::Separator();

    // Layers list
    for (auto& p : layers) {
        const uint16_t id = p.first;
        std::string name = p.second;
        bool visible = lm.IsVisible(id);
        bool locked = lm.IsLocked(id);
        size_t count = lm.EntitiesIn(id).size();

        ImGui::PushID(static_cast<int>(id));
        ImGui::Columns(4, nullptr, false);
        ImGui::SetColumnWidth(0, 40);
        ImGui::Checkbox("##vis", &visible);
        if (visible != lm.IsVisible(id))
            lm.SetVisibility(id, visible);
        ImGui::NextColumn();

        ImGui::SetColumnWidth(1, 200);
        char buf[256]; strncpy_s(buf, name.c_str(), sizeof(buf));
        if (ImGui::InputText("##name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            lm.RenameLayer(id, std::string(buf));
            if (m_fileMenu)
                m_fileMenu->MarkSceneDirty();
        }
        ImGui::NextColumn();

        ImGui::SetColumnWidth(2, 60);
        ImGui::Text("%zu", count);
        ImGui::NextColumn();

        ImGui::SetColumnWidth(3, 120);
        ImGui::Checkbox("Lock", &locked);
        if (locked != lm.IsLocked(id))
            lm.SetLocked(id, locked);
        ImGui::Columns(1);

        // Drag & Drop source/target for reordering
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("LAYER_PAYLOAD", &id, sizeof(id));
            ImGui::Text("%s", name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LAYER_PAYLOAD")) {
                uint16_t fromId = *(const uint16_t*)payload->Data;

                if (fromId != id) {
                    if (m_entityActions)
                        m_entityActions->MoveLayer(fromId, id);
                    else { 
                        lm.MoveLayer(fromId, id); 
                        if (m_fileMenu) 
                            m_fileMenu->MarkSceneDirty(); 
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Row actions: Up/Down reorder, Select, Assign
        ImGui::SameLine();
        if (ImGui::Button("▲")) {
            // Move layer up (decrease index)
            if (id > 0) {
                if (m_entityActions)
                    m_entityActions->MoveLayer(id, id - 1);
                else {
                    lm.MoveLayer(id, id - 1); 
                    if (m_fileMenu) 
                        m_fileMenu->MarkSceneDirty(); 
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("▼")) {
            // Move layer down (increase index)
            if (m_entityActions)
                m_entityActions->MoveLayer(id, id + 1);
            else { 
                lm.MoveLayer(id, id + 1); 
                if (m_fileMenu) 
                    m_fileMenu->MarkSceneDirty(); 
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Select")) {
            if (m_world) {
                if (m_hierarchy) {
                    const auto& ents = lm.EntitiesIn(id);
                    std::unordered_set<uint32_t> sel;

                    for (const auto& e : ents)
                        sel.insert(e.Index);

                    m_hierarchy->SetSelectedEntities(sel);
                }
                else {
                    LOG_INFO("Select all entities in layer " << name << " (" << id << ")");
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Assign Selected") && m_world) {
            if (m_hierarchy && m_scene && m_entityActions) {
                const auto selected = m_hierarchy->GetSelectedEntities();
                bool isDirty = false;

                for (uint32_t sid : selected) {
                    if (m_entityActions && 
                        m_world->Has<ECS::Components::Layer>({ sid, 0 }) && m_world->Get<ECS::Components::Layer>({ sid, 0 }).Id != id) {
                        m_entityActions->SetLayer(sid, id);
                        isDirty = true;
                    }
                }

                if (m_fileMenu && isDirty)
                    m_fileMenu->MarkSceneDirty();
            }
        }

        ImGui::PopID();
        ImGui::Separator();

    }

    ImGui::End();
    if (m_mainFont)
        ImGui::PopFont();
}
