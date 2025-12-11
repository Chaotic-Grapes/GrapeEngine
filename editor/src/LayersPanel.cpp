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
#include "UndoSystem.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#ifdef ERROR
#undef ERROR
#endif
#endif

#include <fstream>

void LayersPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
}

void LayersPanel::MoveLayer(uint16_t fromId, uint16_t toId) {
    if (!m_scene) return;
    auto &lm = m_scene->GetLayers();
    if (fromId == toId) return;

    lm.MoveLayer(fromId, toId);
    if (m_undoSystem) {
        struct MoveCmd : public Editor::ICommand {
            Scenes::Scene* scene; uint16_t from; uint16_t to;
            MoveCmd(Scenes::Scene* s, uint16_t f, uint16_t t) : scene(s), from(f), to(t) {}
            void Execute() override { if (!scene) return; scene->GetLayers().MoveLayer(from, to); }
            void Undo() override { if (!scene) return; scene->GetLayers().MoveLayer(to, from); }
        };

        auto cmd = std::make_unique<MoveCmd>(m_scene, fromId, toId);
        m_undoSystem->ExecuteCommand(std::move(cmd));
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

    try {
        _renderHeader();
        _renderLayersList();
        _renderCollisionMatrix();
        _renderImportConfirm();
        _renderStatusPopup();
    }
    catch (const std::exception &e) {
        LOG_ERROR("Exception in LayersPanel::Render(): " << e.what());
        m_statusMessage = std::string("Render error: ") + e.what();
        m_statusType = LayersPanel::StatusType::Error;
    }

    ImGui::End();
    if (m_mainFont) ImGui::PopFont();
}

LayersPanel::~LayersPanel() {
    // Cleanup subscriptions if any
    if (m_scene) {
        // If we had a messaging subscription system, we would unsubscribe here.
        // Currently nothing to do; keep destructor out-of-line to satisfy linker.
    }
}

void LayersPanel::SetScene(Scenes::Scene* scene) {
    m_scene = scene;
    m_pendingImport.clear();
    m_showImportConfirm = false;
    m_statusMessage.clear();
    m_renameBuffers.clear();  // Clear rename buffers when scene changes
}

void LayersPanel::SetLayer(EntityId entity, uint16_t layerId) {
    if (!m_scene) return;
    ECS::Entity e{ entity, 0 };
    if (e.IsNull()) return;
    m_scene->SetLayer(e, layerId);
    if (m_fileMenu) m_fileMenu->MarkSceneDirty();
}

// Draws the given text as vertically stacked characters (visual 90° clockwise)
// This is a portable substitute for rotated text (ImGui has no built-in text rotation).
static void DrawVerticalTextClockwise(const std::string &text) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float colWidth = ImGui::GetColumnWidth();
    // Use a slightly smaller font so stacked text doesn't crowd the header
    float fontSize = ImGui::GetFontSize() * 0.72f;
    ImFont* font = ImGui::GetFont();
    size_t len = text.size();
    float lineGap = 2.0f; // small gap between stacked chars
    float totalHeight = (fontSize + lineGap) * static_cast<float>(len);

    // Reserve vertical layout space so subsequent items align correctly.
    ImGui::Dummy(ImVec2(0.0f, totalHeight));

    float startX = cursor.x + colWidth * 0.5f;
    float startY = cursor.y;
    for (size_t i = 0; i < len; ++i) {
        char buf[2] = { text[i], '\0' };
        ImVec2 ts = ImGui::CalcTextSize(buf, nullptr, false, 0.0f);
        ImVec2 pos(startX - ts.x * 0.5f, startY + (fontSize + lineGap) * static_cast<float>(i));
        draw->AddText(font, fontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), buf);
    }
}

// --- Render helper implementations ---
void LayersPanel::_renderHeader() {
    auto& lm = m_scene->GetLayers();

    // Header: new layer
    ImGui::InputText("##NewLayerName", m_newLayerName, sizeof(m_newLayerName));
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        std::string nm(m_newLayerName);
        if (!nm.empty()) {
            uint16_t id = lm.CreateLayer(nm);
            if (m_undoSystem) {
                struct CreateLayerCmd : public Editor::ICommand {
                    Scenes::Scene* scene;
                    uint16_t id;
                    std::string name;
                    CreateLayerCmd(Scenes::Scene* s, uint16_t i, std::string n) : scene(s), id(i), name(std::move(n)) {}
                    void Execute() override { if (!scene) return; scene->GetLayers().CreateLayerAt(id, name); }
                    void Undo() override { if (!scene) return; scene->GetLayers().RemoveLayer(id); }
                };
                auto cmd = std::make_unique<CreateLayerCmd>(m_scene, id, nm);
                m_undoSystem->ExecuteCommand(std::move(cmd));
            }
            if (m_fileMenu) m_fileMenu->MarkSceneDirty();
        }
        ImGui::Separator();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (m_fileMenu) {
            m_fileMenu->SaveScene();
            m_statusMessage = "Scene saved (layers persisted).";
            m_statusType = LayersPanel::StatusType::Success;
        }
        else {
            nlohmann::json j = nlohmann::json::array();
            auto layersListE = lm.ListLayers();
            for (const auto& p : layersListE) {
                nlohmann::json e;
                e["id"] = p.first;
                e["name"] = p.second;
                e["mask"] = lm.GetLayerMask(p.first);
                e["visible"] = lm.IsVisible(p.first);
                e["locked"] = lm.IsLocked(p.first);
                j.push_back(e);
            }
            std::string s = j.dump(2);
#ifdef _WIN32
            char filename[MAX_PATH] = "";
            OPENFILENAMEA ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFilter = "JSON files\0*.json\0All files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            ofn.lpstrDefExt = "json";
            if (GetSaveFileNameA(&ofn)) {
                std::ofstream ofs(ofn.lpstrFile, std::ios::binary);
                if (ofs) ofs << s;
            }
#endif
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        // Snapshot current state
        std::vector<std::string> prevNames;
        std::vector<uint32_t> prevMasks;
        std::vector<bool> prevVis;
        std::vector<bool> prevLocked;
        auto list = lm.ListLayers();
        uint16_t maxId = 0; for (auto &p : list) maxId = std::max<uint16_t>(maxId, p.first);
        prevNames.resize(maxId + 1);
        prevMasks.resize(maxId + 1);
        prevVis.resize(maxId + 1);
        prevLocked.resize(maxId + 1);
        for (uint16_t i = 0; i <= maxId; ++i) {
            std::string name;
            for (auto &pp : list) { if (pp.first == i) { name = pp.second; break; } }
            prevNames[i] = name;
            prevMasks[i] = lm.GetLayerMask(i);
            prevVis[i] = lm.IsVisible(i);
            prevLocked[i] = lm.IsLocked(i);
        }

        // Snapshot entity layer assignments
        std::vector<std::pair<uint32_t, uint16_t>> entityLayers;
        ECS::World& world = m_scene->GetWorld();
        world.Each([&](ECS::Entity e) {
            if (world.Has<ECS::Components::Layer>(e)) entityLayers.emplace_back(e.Index, world.Get<ECS::Components::Layer>(e).Id);
        });

        // Apply reset
        lm.ResetToDefaults();

        if (m_undoSystem) {
            struct ResetCmd : public Editor::ICommand {
                Scenes::Scene* scene;
                std::vector<std::string> names;
                std::vector<uint32_t> masks;
                std::vector<bool> vis;
                std::vector<bool> locked;
                std::vector<std::pair<uint32_t, uint16_t>> entityLayers;
                ResetCmd(Scenes::Scene* s, std::vector<std::string> n, std::vector<uint32_t> m, std::vector<bool> v, std::vector<bool> l, std::vector<std::pair<uint32_t, uint16_t>> el)
                    : scene(s), names(std::move(n)), masks(std::move(m)), vis(std::move(v)), locked(std::move(l)), entityLayers(std::move(el)) {}
                void Execute() override { if (!scene) return; scene->GetLayers().ResetToDefaults(); }
                void Undo() override {
                    if (!scene) return;
                    auto &lm = scene->GetLayers();
                    for (uint16_t i = 0; i < names.size(); ++i) {
                        if (!names[i].empty()) { lm.CreateLayerAt(i, names[i]); lm.SetLayerMask(i, masks[i]); lm.SetVisibility(i, vis[i]); lm.SetLocked(i, locked[i]); }
                        else lm.RemoveLayer(i);
                    }
                    ECS::World& world = scene->GetWorld();
                    for (auto &p : entityLayers) {
                        ECS::Entity e = world.Resolve(p.first);
                        if (!e.IsNull() && world.IsAlive(e)) scene->SetLayer(e, p.second);
                    }
                }
            };

            auto cmd = std::make_unique<ResetCmd>(m_scene, std::move(prevNames), std::move(prevMasks), std::move(prevVis), std::move(prevLocked), std::move(entityLayers));
            m_undoSystem->ExecuteCommand(std::move(cmd));
        }

        if (m_fileMenu) m_fileMenu->MarkSceneDirty();
        m_statusMessage = "Layers reset to default.";
        m_statusType = LayersPanel::StatusType::Info;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
#ifdef _WIN32
        char filename[MAX_PATH] = "";
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFilter = "JSON files\0*.json\0All files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            std::ifstream ifs(ofn.lpstrFile, std::ios::binary);
            if (ifs) {
                std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                try {
                    auto j = nlohmann::json::parse(content);
                    if (j.is_object() && j.contains("Layers")) j = j["Layers"];
                    if (j.is_array()) {
                        // Clear any previous pending import
                        m_pendingImport.clear();

                        for (auto &e : j) {
                            if (e.contains("id") && e.contains("name")) {
                                uint16_t id = static_cast<uint16_t>(e["id"].get<int>());
                                std::string name = e["name"].get<std::string>();

                                LayersPanel::LayerImportEntry entry;
                                entry.Id = id;
                                entry.Name = name;

                                if (e.contains("mask")) {
                                    entry.Mask = static_cast<uint32_t>(e["mask"].get<uint32_t>());
                                    entry.HasMask = true;
                                }
                                if (e.contains("visible")) {
                                    entry.Visible = e["visible"].get<bool>();
                                    entry.HasVisible = true;
                                }
                                if (e.contains("locked")) {
                                    entry.Locked = e["locked"].get<bool>();
                                    entry.HasLocked = true;
                                }

                                // Try to create at the specified index. If the slot is free, apply immediately.
                                if (lm.CreateLayerAt(entry.Id, entry.Name)) {
                                    if (entry.HasMask) lm.SetLayerMask(entry.Id, entry.Mask);
                                    if (entry.HasVisible) lm.SetVisibility(entry.Id, entry.Visible);
                                    if (entry.HasLocked) lm.SetLocked(entry.Id, entry.Locked);
                                }
                                else {
                                    // Slot is occupied: defer to pending import and ask user
                                    m_pendingImport.push_back(entry);
                                }
                            }
                        }

                        if (!m_pendingImport.empty()) {
                            m_showImportConfirm = true;
                            ImGui::OpenPopup("OverwriteLayersConfirm");
                        }

                        if (m_fileMenu) m_fileMenu->MarkSceneDirty();
                    }
                }
                catch (...) { /* ignore parse errors */ }
            }
        }
#endif
    }

    // Tooltips for Save/Load
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Saves/loads both the layer definitions and the collision matrix.");
}

void LayersPanel::_renderCollisionMatrix() {
    auto& lm = m_scene->GetLayers();
    ImGui::Separator();
    if (!ImGui::CollapsingHeader("Collision Matrix", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if (ImGui::Button("Reset Matrix")) {
        auto layersListR = lm.ListLayers();
        std::vector<std::pair<uint16_t, uint32_t>> prev;
        prev.reserve(layersListR.size());
        for (const auto& p : layersListR) prev.emplace_back(p.first, lm.GetLayerMask(p.first));
        for (const auto& p : layersListR) lm.SetLayerMask(p.first, 0xFFFFFFFFu);
        if (m_undoSystem) {
            struct BatchMaskCmd : public Editor::ICommand {
                Scenes::Scene* scene;
                std::vector<std::pair<uint16_t, uint32_t>> prev;
                BatchMaskCmd(Scenes::Scene* s, std::vector<std::pair<uint16_t, uint32_t>> p) : scene(s), prev(std::move(p)) {}
                void Execute() override { }
                void Undo() override { if (!scene) return; auto &lm = scene->GetLayers(); for (auto &pr : prev) lm.SetLayerMask(pr.first, pr.second); }
            };
            auto cmd = std::make_unique<BatchMaskCmd>(m_scene, std::move(prev));
            m_undoSystem->ExecuteCommand(std::move(cmd));
        }
        if (m_fileMenu) m_fileMenu->MarkSceneDirty();
    }

    auto layersList = lm.ListLayers();
    const int count = static_cast<int>(layersList.size());
    if (count <= 0) return;

    // Fill remaining available panel height for the matrix child
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("LayerMatrixChild", ImVec2(0, avail.y > 50.0f ? avail.y : 50.0f), true);
    ImGui::Columns(count + 1, "layer_matrix_cols");
    ImGui::Text("\n");
    ImGui::NextColumn();
    for (int c = 0; c < count; ++c) {
        const auto& p = layersList[c];
        DrawVerticalTextClockwise(p.second);
        ImGui::NextColumn();
    }

    for (int r = 0; r < count; ++r) {
        const uint16_t rowId = layersList[r].first;
        ImGui::TextWrapped("%s", layersList[r].second.c_str());
        ImGui::NextColumn();

        for (int c = 0; c < count; ++c) {
            const uint16_t colId = layersList[c].first;
            uint32_t rowMask = lm.GetLayerMask(rowId);
            const uint32_t bit = (colId < 32) ? (1u << colId) : 0u;
            bool collides = (rowMask & bit) != 0u;

            ImGui::PushID((int)rowId * 1000 + (int)colId);
            if (rowId == colId) {
                ImGui::BeginDisabled();
                ImGui::Checkbox("##cell", &collides);
                ImGui::EndDisabled();
                ImGui::NextColumn();
            } else {
                bool prevState = collides;
                ImGui::Checkbox("##cell", &collides);
                if (collides != prevState) {
                    uint32_t prevRowMask = lm.GetLayerMask(rowId);
                    uint32_t prevColMask = lm.GetLayerMask(colId);

                    const uint32_t bitForCol = (colId < 32) ? (1u << colId) : 0u;
                    const uint32_t bitForRow = (rowId < 32) ? (1u << rowId) : 0u;

                    uint32_t newRowMask = prevRowMask;
                    uint32_t newColMask = prevColMask;

                    if (collides) { newRowMask |= bitForCol; newColMask |= bitForRow; }
                    else { newRowMask &= ~bitForCol; newColMask &= ~bitForRow; }

                    lm.SetLayerMask(rowId, newRowMask);
                    lm.SetLayerMask(colId, newColMask);

                    if (m_undoSystem) {
                        // Command
                        struct CollisionCmd : public Editor::ICommand {
                            Scenes::Scene* scene; uint16_t a; uint16_t b; uint32_t prevA; uint32_t prevB; uint32_t nextA; uint32_t nextB;
                            CollisionCmd(Scenes::Scene* s, uint16_t aa, uint16_t bb, uint32_t pA, uint32_t pB, uint32_t nA, uint32_t nB)
                                : scene(s), a(aa), b(bb), prevA(pA), prevB(pB), nextA(nA), nextB(nB) {}

                            void Execute() override {
                                if (!scene)
                                    return;

                                scene->GetLayers().SetLayerMask(a, nextA);
                                scene->GetLayers().SetLayerMask(b, nextB);
                            }

                            void Undo() override {
                                if (!scene)
                                    return;

                                scene->GetLayers().SetLayerMask(a, prevA);
                                scene->GetLayers().SetLayerMask(b, prevB);
                            }
                        };
                        auto cmd = std::make_unique<CollisionCmd>(m_scene, rowId, colId, prevRowMask, prevColMask, newRowMask, newColMask);
                        m_undoSystem->ExecuteCommand(std::move(cmd));
                    }
                    if (m_fileMenu) m_fileMenu->MarkSceneDirty();
                }
                ImGui::NextColumn();
            }
            ImGui::PopID();
        }
    }

    ImGui::Columns(1);
    ImGui::EndChild();
}

void LayersPanel::_renderLayersList() {
    auto& lm = m_scene->GetLayers();
    auto layers = lm.ListLayers();

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
        if (visible != lm.IsVisible(id)) {
            bool prev = lm.IsVisible(id);
            lm.SetVisibility(id, visible);

            if (m_undoSystem) {
                // Command
                struct VisCmd : public Editor::ICommand {
                    Scenes::Scene* scene; uint16_t id; bool prev; bool next;
                    VisCmd(Scenes::Scene* s, uint16_t i, bool p, bool n) : scene(s), id(i), prev(p), next(n) {}

                    void Execute() override {
                        if (!scene)
                            return;

                        scene->GetLayers().SetVisibility(id, next); 
                    }

                    void Undo() override {
                        if (!scene)
                            return;

                        scene->GetLayers().SetVisibility(id, prev); 
                    }
                };

                auto cmd = std::make_unique<VisCmd>(m_scene, id, prev, visible);
                m_undoSystem->ExecuteCommand(std::move(cmd));
            }
        }
        ImGui::NextColumn();

        ImGui::SetColumnWidth(1, 200);

        // Initialize or retrieve the persistent rename buffer for this layer
        if (m_renameBuffers.find(id) == m_renameBuffers.end()) {
            m_renameBuffers[id] = {};
            strcpy_s(m_renameBuffers[id].data(), m_renameBuffers[id].size(), name.c_str());
        }

        char* buf = m_renameBuffers[id].data();
        if (ImGui::InputText("##name", buf, m_renameBuffers[id].size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string newName(buf);
            // Only proceed if the name actually changed
            if (newName != name) {
                std::string prevName = name;
                lm.RenameLayer(id, newName);

                // Update the buffer to reflect the new name from the layer system
                strcpy_s(m_renameBuffers[id].data(), m_renameBuffers[id].size(), newName.c_str());

                if (m_undoSystem) {
                    // Command
                    struct RenameCmd : public Editor::ICommand {
                        Scenes::Scene* scene; uint16_t id; std::string prev; std::string next;
                        RenameCmd(Scenes::Scene* s, uint16_t i, std::string p, std::string n) : scene(s), id(i), prev(std::move(p)), next(std::move(n)) {}
                        void Execute() override {
                            if (!scene)
                                return;
                            scene->GetLayers().RenameLayer(id, next);
                        }

                        void Undo() override {
                            if (!scene)
                                return;
                            scene->GetLayers().RenameLayer(id, prev);
                        }
                    };

                    auto cmd = std::make_unique<RenameCmd>(m_scene, id, prevName, newName);
                    m_undoSystem->ExecuteCommand(std::move(cmd));
                }

                if (m_fileMenu) m_fileMenu->MarkSceneDirty();
            }
        }
        ImGui::NextColumn();

        ImGui::SetColumnWidth(2, 60);
        ImGui::Text("%zu", count);
        ImGui::NextColumn();

        ImGui::SetColumnWidth(3, 120);
        ImGui::Checkbox("Lock", &locked);
        if (locked != lm.IsLocked(id)) {
            bool prev = lm.IsLocked(id);
            lm.SetLocked(id, locked);

            if (m_undoSystem) {
                // Command
                struct LockCmd : public Editor::ICommand {
                    Scenes::Scene* scene; uint16_t id; bool prev; bool next;
                    LockCmd(Scenes::Scene* s, uint16_t i, bool p, bool n) : scene(s), id(i), prev(p), next(n) {}
                    void Execute() override { if (!scene) return; scene->GetLayers().SetLocked(id, next); }
                    void Undo() override { if (!scene) return; scene->GetLayers().SetLocked(id, prev); }
                };
                auto cmd = std::make_unique<LockCmd>(m_scene, id, prev, locked);
                m_undoSystem->ExecuteCommand(std::move(cmd));
            }
        }
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

                if (fromId != id && m_scene) {
                    // Determine source and destination indices within the current visible list
                    auto layersListLocal = lm.ListLayers();
                    int srcIdx = -1, dstIdx = -1;
                    for (size_t i = 0; i < layersListLocal.size(); ++i) {
                        if (layersListLocal[i].first == fromId) srcIdx = static_cast<int>(i);
                        if (layersListLocal[i].first == id) dstIdx = static_cast<int>(i);
                    }

                    if (srcIdx != -1 && dstIdx != -1 && srcIdx != dstIdx && m_undoSystem) {
                        struct MoveCmd : public Editor::ICommand {
                            Scenes::Scene* scene; int fromIndex; int toIndex;
                            MoveCmd(Scenes::Scene* s, int f, int t) : scene(s), fromIndex(f), toIndex(t) {}

                            void Execute() override {
                                if (!scene)
                                    return;

                                auto &lm = scene->GetLayers();
                                auto layers = lm.ListLayers();

                                if (fromIndex < 0 || toIndex < 0 ||
                                    fromIndex >= (int)layers.size() || toIndex >= (int)layers.size())
                                    return;

                                // perform adjacent swaps to move element
                                if (fromIndex < toIndex) {
                                    for (int i = fromIndex; i < toIndex; ++i) {
                                        auto cur = lm.ListLayers();
                                        lm.SwapLayers(cur[i].first, cur[i+1].first);
                                    }
                                }
                                else if (fromIndex > toIndex) {
                                    for (int i = fromIndex; i > toIndex; --i) {
                                        auto cur = lm.ListLayers();
                                        lm.SwapLayers(cur[i].first, cur[i-1].first);
                                    }
                                }
                            }

                            void Undo() override {
                                if (!scene)
                                    return;
                                auto &lm = scene->GetLayers();

                                // reverse the same swaps using current ordering
                                if (fromIndex < toIndex) {
                                    for (int i = toIndex; i > fromIndex; --i) {
                                        auto cur = lm.ListLayers();
                                        lm.SwapLayers(cur[i].first, cur[i-1].first);
                                    }
                                }
                                else if (fromIndex > toIndex) {
                                    for (int i = toIndex; i < fromIndex; ++i) {
                                        auto cur = lm.ListLayers();
                                        lm.SwapLayers(cur[i].first, cur[i+1].first);
                                    }
                                }
                            }
                        };

                        auto cmd = std::make_unique<MoveCmd>(m_scene, srcIdx, dstIdx);
                        m_undoSystem->ExecuteCommand(std::move(cmd));
                        if (m_fileMenu) m_fileMenu->MarkSceneDirty();
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Row actions: Up/Down reorder, Select, Assign
        ImGui::SameLine();
        ImGui::SameLine();
        if (ImGui::Button("▲")) {
            // Move current entry one slot up using SwapLayers via an undoable command
            int currentIdx = -1;
            auto curList = lm.ListLayers();
            for (size_t i = 0; i < curList.size(); ++i) if (curList[i].first == id) { currentIdx = static_cast<int>(i); break; }
            if (currentIdx > 0 && m_undoSystem) {
                struct MoveCmd : public Editor::ICommand {
                    Scenes::Scene* scene; int fromIndex; int toIndex;
                    MoveCmd(Scenes::Scene* s, int f, int t) : scene(s), fromIndex(f), toIndex(t) {}
                    void Execute() override {
                        if (!scene) return;
                        auto &lm = scene->GetLayers();
                        // perform adjacent swaps to move element
                        for (int i = fromIndex; i > toIndex; --i) {
                            auto cur = lm.ListLayers();
                            lm.SwapLayers(cur[i].first, cur[i-1].first);
                        }
                    }
                    void Undo() override {
                        if (!scene) return;
                        auto &lm = scene->GetLayers();
                        for (int i = toIndex; i < fromIndex; ++i) {
                            auto cur = lm.ListLayers();
                            lm.SwapLayers(cur[i].first, cur[i+1].first);
                        }
                    }
                };
                auto cmd = std::make_unique<MoveCmd>(m_scene, currentIdx, currentIdx - 1);
                m_undoSystem->ExecuteCommand(std::move(cmd));
                if (m_fileMenu) m_fileMenu->MarkSceneDirty();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("▼")) {
            int currentIdx = -1;
            auto curList = lm.ListLayers();
            for (size_t i = 0; i < curList.size(); ++i) if (curList[i].first == id) { currentIdx = static_cast<int>(i); break; }
            if (currentIdx >= 0 && currentIdx < static_cast<int>(curList.size()) - 1 && m_undoSystem) {
                struct MoveCmd : public Editor::ICommand {
                    Scenes::Scene* scene; int fromIndex; int toIndex;
                    MoveCmd(Scenes::Scene* s, int f, int t) : scene(s), fromIndex(f), toIndex(t) {}
                    void Execute() override {
                        if (!scene) return;
                        auto &lm = scene->GetLayers();
                        for (int i = fromIndex; i < toIndex; ++i) {
                            auto cur = lm.ListLayers();
                            lm.SwapLayers(cur[i].first, cur[i+1].first);
                        }
                    }
                    void Undo() override {
                        if (!scene) return;
                        auto &lm = scene->GetLayers();
                        for (int i = toIndex; i > fromIndex; --i) {
                            auto cur = lm.ListLayers();
                            lm.SwapLayers(cur[i].first, cur[i-1].first);
                        }
                    }
                };
                auto cmd = std::make_unique<MoveCmd>(m_scene, currentIdx, currentIdx + 1);
                m_undoSystem->ExecuteCommand(std::move(cmd));
                if (m_fileMenu) m_fileMenu->MarkSceneDirty();
            }
        }
        ImGui::PopID();
    }
}

void LayersPanel::_renderImportConfirm() {
    if (m_showImportConfirm) {
        ImGui::OpenPopup("OverwriteLayersConfirm");
        ImGui::SetNextWindowSize(ImVec2(500, 260));
    }

    if (!ImGui::BeginPopupModal("OverwriteLayersConfirm", nullptr, ImGuiWindowFlags_NoResize)) return;

    ImGui::TextWrapped("The import contains layers whose IDs are already in use. Overwrite existing layers?");
    ImGui::Separator();

    ImGui::BeginChild("PendingImportList", ImVec2(0, 140), true);
    for (const auto &entry : m_pendingImport) {
        ImGui::Text("%d: %s   mask=0x%08X   vis=%s   locked=%s",
            entry.Id,
            entry.Name.c_str(),
            entry.Mask,
            entry.HasVisible ? (entry.Visible ? "true" : "false") : "n/a",
            entry.HasLocked ? (entry.Locked ? "true" : "false") : "n/a");
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Apply")) {
        auto& lm = m_scene->GetLayers();
        std::vector<std::tuple<uint16_t, std::string, uint32_t, bool, bool>> prev;
        prev.reserve(m_pendingImport.size());

        for (const auto &entry : m_pendingImport) {
            std::string pname;
            auto lst = lm.ListLayers();
            for (auto &p : lst) if (p.first == entry.Id) { pname = p.second; break; }
            prev.emplace_back(entry.Id, pname, lm.GetLayerMask(entry.Id), lm.IsVisible(entry.Id), lm.IsLocked(entry.Id));
        }

        for (const auto &entry : m_pendingImport) {
            if (!lm.CreateLayerAt(entry.Id, entry.Name)) lm.RenameLayer(entry.Id, entry.Name);
            if (entry.HasMask) lm.SetLayerMask(entry.Id, entry.Mask);
            if (entry.HasVisible) lm.SetVisibility(entry.Id, entry.Visible);
            if (entry.HasLocked) lm.SetLocked(entry.Id, entry.Locked);
        }

        if (m_undoSystem) {
            struct BatchCmd : public Editor::ICommand {
                Scenes::Scene* scene;
                std::vector<std::tuple<uint16_t, std::string, uint32_t, bool, bool>> prev;
                BatchCmd(Scenes::Scene* s, std::vector<std::tuple<uint16_t, std::string, uint32_t, bool, bool>> p)
                    : scene(s), prev(std::move(p)) {}

                void Execute() override { }
                void Undo() override {
                    if (!scene)
                        return;

                    auto &lm = scene->GetLayers();
                    for (const auto &p : prev) {
                        uint16_t id = std::get<0>(p);
                        const std::string &name = std::get<1>(p);

                        uint32_t mask = std::get<2>(p);
                        bool vis = std::get<3>(p);
                        bool locked = std::get<4>(p);

                        if (!name.empty()) {
                            if (!lm.CreateLayerAt(id, name)) lm.RenameLayer(id, name);
                            lm.SetLayerMask(id, mask);
                            lm.SetVisibility(id, vis);
                            lm.SetLocked(id, locked);
                        }
                        else {
                            lm.RemoveLayer(id);
                        }
                    }
                }
            };

            auto cmd = std::make_unique<BatchCmd>(m_scene, std::move(prev));
            m_undoSystem->ExecuteCommand(std::move(cmd));
        }

        if (m_fileMenu) m_fileMenu->MarkSceneDirty();
        m_pendingImport.clear();
        m_showImportConfirm = false;
        m_statusMessage = "Layers imported successfully.";
        m_statusType = LayersPanel::StatusType::Success;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        m_pendingImport.clear();
        m_showImportConfirm = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void LayersPanel::_renderStatusPopup() {
    if (m_statusMessage.empty()) return;
    ImGui::OpenPopup("LayersStatus");

    if (!ImGui::BeginPopupModal("LayersStatus", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (m_statusType == LayersPanel::StatusType::Success) {
        ImGui::TextColored(ImVec4(0.15f, 0.7f, 0.15f, 1.0f), "%s", m_statusMessage.c_str());
    } else if (m_statusType == LayersPanel::StatusType::Error) {
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%s", m_statusMessage.c_str());
    } else {
        ImGui::Text("%s", m_statusMessage.c_str());
    }

    if (ImGui::Button("OK")) {
        m_statusMessage.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
