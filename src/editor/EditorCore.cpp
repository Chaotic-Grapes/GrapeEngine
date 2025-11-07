/* Start Header *****************************************************************/
/*!
\file   EditorCore.cpp
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implements the EditorCore class for core editor functionality and entity management.
*/
/* End Header *******************************************************************/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include "../editor/EditorCore.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "services/Input.h"
#include <imgui.h>
#include <algorithm>
#include "helpers/MathHelper.h"
#include "services/OverlayService.h"
#include "core/Application.h"
#include "scene/Scene.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"

static constexpr const char* LEVEL_DIR = "assets/levels/";
static constexpr const char* SCENE_DIR = "assets/scenes/";
static constexpr const char* SCENE_TEMPLATE_DIR = "assets/scenes/templates/";

void EditorCore::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    _loadEditorState();
}

void EditorCore::HandleInWorldInteraction() {
    if (!HasValidWorld()) return;

    static bool isDragging = false;

    if (ImGui::GetIO().WantCaptureMouse) return;

    double xPos, yPos;
    Input::GetMousePosition(xPos, yPos);
    Vector2D mouseWorldPos(static_cast<float>(xPos), static_cast<float>(yPos));

    if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        EntityId newSelection = 0;

        std::vector<ECS::Entity> pickableEntities;
        m_world->Each<ECS::Components::LocalTransform, ECS::Components::CircleCollider2D>(
            [&](ECS::Entity e, ECS::Components::LocalTransform&, ECS::Components::CircleCollider2D&) {
                pickableEntities.push_back(e);
            }
        );

        for (auto it = pickableEntities.rbegin(); it != pickableEntities.rend(); it++) {
            ECS::Entity entity = *it;

            const auto& transform = m_world->Get<ECS::Components::LocalTransform>(entity);
            const auto& collider = m_world->Get<ECS::Components::CircleCollider2D>(entity);

            float distance = MathHelper::Distance(
                Vector2D(transform.Position.X, transform.Position.Y),
                mouseWorldPos
            );

            if (distance <= collider.Radius) {
                newSelection = entity.Index;
                isDragging = true;
                break;
            }
        }

        if (m_selectedEntityId != newSelection) {
            m_selectedEntityId = newSelection;
            _invalidateCache();
        }
    }

    if (isDragging && m_selectedEntityId != 0 && Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        ECS::Entity selectedEntity{ m_selectedEntityId, 0 };

        if (m_world->IsAlive(selectedEntity) && m_world->Has<ECS::Components::LocalTransform>(selectedEntity)) {
            auto& transform = m_world->Get<ECS::Components::LocalTransform>(selectedEntity);
            transform.Position.X = mouseWorldPos.X;
            transform.Position.Y = mouseWorldPos.Y;
        }
    }

    if (isDragging && !Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        isDragging = false;
    }
}

void EditorCore::ShowEditorWindows() {
    bool ctrlDown = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
    bool shiftDown = Input::IsKeyDown(KEY_LEFT_SHIFT) || Input::IsKeyDown(KEY_RIGHT_SHIFT);

    if (ctrlDown) {
        if (Input::IsKeyPressed(KEY_EQUAL) || Input::IsKeyPressed(GLFW_KEY_KP_ADD)) {
            m_uiScale += 0.10f;
        }
        if (Input::IsKeyPressed(KEY_MINUS) || Input::IsKeyPressed(GLFW_KEY_KP_SUBTRACT)) {
            m_uiScale -= 0.10f;
        }
        if (Input::IsKeyPressed(KEY_N)) {
            _createNewScene();
        }
        if (Input::IsKeyPressed(KEY_O)) {
            _openSceneDialog();
        }
        if (Input::IsKeyPressed(KEY_S)) {
            if (shiftDown) {
                _saveSceneAsDialog(false);
            }
            else {
                _saveScene();
            }
        }
    }

    m_uiScale = std::clamp(m_uiScale, 0.75f, 2.0f);
    ImGui::GetIO().FontGlobalScale = m_uiScale;
    _showMainMenu();
    _showViewport();
}

void EditorCore::_showMainMenu() {
    // Style overrides for main menu bar: darker background and no border
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::Text("UI Scale: %.0f%%", m_uiScale * 100.0f);
            ImGui::Separator();

            if (ImGui::MenuItem("Zoom In", "Ctrl++")) {
                m_uiScale += 0.10f;
            }
            if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) {
                m_uiScale -= 0.10f;
            }

            if (ImGui::BeginMenu("Presets")) {
                auto preset = [&](float v, const char* label) {
                    if (ImGui::MenuItem(label, nullptr, std::abs(m_uiScale - v) < 0.001f)) m_uiScale = v;
                    };
                preset(1.00f, "100%");
                preset(1.15f, "115%");
                preset(1.25f, "125%");
                preset(1.35f, "135% (Default)");
                preset(1.50f, "150%");
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Reset to 135%")) {
                m_uiScale = 1.35f;
            }

            m_uiScale = std::clamp(m_uiScale, 0.75f, 2.0f);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                _createNewScene();
            }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
                _openSceneDialog();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                _saveScene();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                _saveSceneAsDialog(false);
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Templates")) {
                if (ImGui::MenuItem("Save as Template")) {
                    _saveSceneAsDialog(true);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Restore the menu bar style var and color pushed above
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void EditorCore::_showViewport() {
    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Viewport");

    // Render scene using editor camera
    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        auto* activeScene = sm.GetActive();

        if (activeScene) {
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();

            // Get the scene's render texture if available
            // This is where the actual game/editor view is rendered
            ImGui::Image(
                (void*)(intptr_t)0,  // Replace with actual texture ID from renderer
                viewportSize,
                ImVec2(0, 1),
                ImVec2(1, 0)
            );
        }
        else {
            ImGui::TextDisabled("No scene loaded");
            ImGui::TextDisabled("Click on File to create a new scene or open an existing one");
        }
    }

    ImGui::End();
    ImGui::PopFont();
}

void EditorCore::AddEntity(const std::string& name, EntityId parentId) {
    if (!HasValidWorld()) return;

    ECS::Entity newEntity = m_world->Create();

    auto& nameComp = m_world->Add<ECS::Components::Name>(newEntity);
    std::strncpy(nameComp.Value, name.c_str(), sizeof(nameComp.Value) - 1);
    nameComp.Value[sizeof(nameComp.Value) - 1] = '\0';

    m_world->Add<ECS::Components::LocalTransform>(newEntity);

    if (parentId != ECS::Entity::NPOS32) {
        // Resolve parent using world's current generation to ensure hierarchy indices match
        ECS::Entity parent = m_world->Resolve(parentId);
        if (!parent.IsNull()) {
            m_world->Add<ECS::Parent>(newEntity, parent);
        }
    }

    LOG_INFO("Created entity: " << name);
}

void EditorCore::ReparentEntity(EntityId childId, EntityId newParentId) {
    if (!HasValidWorld()) return;

    ECS::Entity child = m_world->Resolve(childId);
    ECS::Entity newParent = m_world->Resolve(newParentId);

    if (child.IsNull()) return;
    if (newParentId != ECS::Entity::NPOS32 && newParent.IsNull()) return;

    if (newParentId == ECS::Entity::NPOS32) {
        if (m_world->Has<ECS::Parent>(child)) {
            m_world->Remove<ECS::Parent>(child);
        }
    }
    else {
        // Use Set/Add to ensure world hierarchy indices are updated via change hooks
        if (m_world->Has<ECS::Parent>(child)) {
            m_world->Set<ECS::Parent>(child, ECS::Parent{ newParent });
        }
        else {
            m_world->Add<ECS::Parent>(child, newParent);
        }
    }
}

void EditorCore::RemoveEntity(EntityId id, bool recursive) {
    if (!HasValidWorld()) return;

    ECS::Entity entity{ id, 0 };
    if (!m_world->IsAlive(entity)) return;

    if (recursive) {
        m_world->ForChildren(entity, [&](ECS::Entity child) {
            RemoveEntity(child.Index, true);
            });
    }

    m_world->Destroy(entity);

    if (m_selectedEntityId == id) {
        m_selectedEntityId = 0;
    }
}

void EditorCore::CloneEntity(EntityId id) {
    if (!HasValidWorld()) return;

    ECS::Entity source = m_world->Resolve(id);
    if (source.IsNull()) return;

    // Serialize the entire hierarchy first before creating any clones
    std::function<nlohmann::json(ECS::Entity)> serializeHierarchy = [&](ECS::Entity entity) {
        nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

        // Recursively serialize children
        std::vector<ECS::Entity> children;
        m_world->ForChildren(entity, [&](ECS::Entity child) {
            children.push_back(child);
            });

        if (!children.empty()) {
            entityJson["Children"] = nlohmann::json::array();
            for (const auto& child : children) {
                entityJson["Children"].push_back(serializeHierarchy(child));
            }
        }

        return entityJson;
        };

    nlohmann::json sourceHierarchy = serializeHierarchy(source);

    // Deserialize the entire hierarchy
    std::function<ECS::Entity(const nlohmann::json&, ECS::Entity)> deserializeHierarchy =
        [&](const nlohmann::json& entityJson, ECS::Entity parent) -> ECS::Entity {
        ECS::Entity clonedEntity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);

        // Set parent relationship if provided
        if (!parent.IsNull()) {
            m_world->Add<ECS::Parent>(clonedEntity, parent);
        }

        // Recursively deserialize children
        if (entityJson.contains("Children") && entityJson["Children"].is_array()) {
            for (const auto& childJson : entityJson["Children"]) {
                deserializeHierarchy(childJson, clonedEntity);
            }
        }

        return clonedEntity;
        };

    // Preserve original parent relationship if any
    ECS::Entity originalParent = m_world->ParentOf(source);
    ECS::Entity clone = deserializeHierarchy(sourceHierarchy, originalParent);

    LOG_INFO("Cloned entity " << id << " with hierarchy");
}

void EditorCore::ClearAllEntities() {
    if (!HasValidWorld()) return;

    std::vector<ECS::Entity> allEntities;
    m_world->Each([&](ECS::Entity e) {
        allEntities.push_back(e);
        });

    for (const auto& e : allEntities) {
        m_world->Destroy(e);
    }

    m_selectedEntityId = 0;
    _invalidateCache();
}

bool EditorCore::HasValidWorld() const {
    return m_world != nullptr;
}

void EditorCore::SetWorld(ECS::World* world) {
    m_world = world;
    m_selectedEntityId = 0;
    _invalidateCache();
}

void EditorCore::_invalidateCache() {
    m_cachedDeleteLabels.clear();
    m_cachedCloneLabels.clear();
    m_cachedCollapsedHeaders.clear();
}

const std::string& EditorCore::_getDeleteLabel(const EntityId id) {
    auto it = m_cachedDeleteLabels.find(id);
    if (it == m_cachedDeleteLabels.end()) {
        std::string label = "Delete##" + std::to_string(id);
        it = m_cachedDeleteLabels.insert({ id, label }).first;
    }
    return it->second;
}

const std::string& EditorCore::_getCloneLabel(const EntityId id) {
    auto it = m_cachedCloneLabels.find(id);
    if (it == m_cachedCloneLabels.end()) {
        std::string label = "Clone##" + std::to_string(id);
        it = m_cachedCloneLabels.insert({ id, label }).first;
    }
    return it->second;
}

const bool& EditorCore::_getCollapsedHeaderBool(const EntityId id) {
    auto it = m_cachedCollapsedHeaders.find(id);
    if (it == m_cachedCollapsedHeaders.end()) {
        it = m_cachedCollapsedHeaders.insert({ id, false }).first;
    }
    return it->second;
}

void EditorCore::_createNewScene() {
    LOG_INFO("New Scene requested");

    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        size_t newIdx = sm.AddScene(new Scenes::Scene());
        sm.SetActiveImmediate(newIdx);

        if (auto* overlay = Services::OverlayService::Get()) {
            auto* active = sm.GetActive();
            if (active) overlay->SetWorld(&active->GetWorld());
        }

        // Spawn a default camera entity to match Unity's empty scene behavior
        if (auto* active = sm.GetActive()) {
            ECS::World& world = active->GetWorld();
            ECS::Entity cam = world.Create();

            // Name (optional, if Name component exists)
            if constexpr (true) {
                auto& nameComp = world.Add<ECS::Components::Name>(cam);
                std::strncpy(nameComp.Value, "Main Camera", sizeof(nameComp.Value) - 1);
                nameComp.Value[sizeof(nameComp.Value) - 1] = '\0';
            }

            auto& camTr = world.Add<ECS::Components::LocalTransform>(cam);
            camTr.Position = { 0.f, 0.f, 10.f }; // Z = camera height
            camTr.Scale = { 1.f, 1.f, 1.f };
            camTr.Rotation = Quaternion::Identity();

            auto& camera = world.Add<ECS::Components::Camera3D>(cam);
            camera.Active = true;
            camera.UsePerspective = false; // 2D-style default
            camera.OrthoSize = 16.f;       // sensible starting zoom
            camera.NearPlane = 0.1f;
            camera.FarPlane = 100.f;

            const auto window = WindowManager::GetMainWindow();
            if (window) {
                camera.AspectRatio = static_cast<float>(window->Width()) /
                    static_cast<float>(window->Height());
            }
        }
    }

    m_currentScenePath.clear();
    m_currentSceneName = "Untitled";
}

void EditorCore::_openSceneDialog() {
#ifdef _WIN32
    char filename[512] = "";

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "Scene Files\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Open Scene";
    ofn.lpstrInitialDir = SCENE_DIR;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        std::string path(filename);
        if (_loadSceneFromPath(path)) {
            m_currentScenePath = path;
            m_currentSceneName = std::filesystem::path(path).stem().string();
            _saveEditorState();
            LOG_INFO("Loaded scene: " << m_currentSceneName);
        }
    }
#else
    LOG_WARNING("File dialogs not supported on this platform");
#endif
}

void EditorCore::_saveScene() {
    if (m_currentScenePath.empty()) {
        _saveSceneAsDialog(false);
    }
    else {
        if (_saveActiveScene(m_currentScenePath)) {
            _saveEditorState();
            LOG_INFO("Saved scene: " << m_currentSceneName);
        }
    }
}

void EditorCore::_saveSceneAsDialog(bool isTemplate) {
#ifdef _WIN32
    char filename[512] = "";
    if (!m_currentSceneName.empty() && m_currentSceneName != "Untitled") {
        strncpy_s(filename, m_currentSceneName.c_str(), sizeof(filename) - 1);
    }

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "Scene Files\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = isTemplate ? "Save Scene Template" : "Save Scene As";
    ofn.lpstrInitialDir = isTemplate ? SCENE_TEMPLATE_DIR : SCENE_DIR;
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        std::string path(filename);
        std::filesystem::path dirPath = std::filesystem::path(path).parent_path();
        std::filesystem::create_directories(dirPath);

        if (_saveActiveScene(path)) {
            m_currentScenePath = path;
            m_currentSceneName = std::filesystem::path(path).stem().string();
            _saveEditorState();
            LOG_INFO("Saved scene as: " << m_currentSceneName);
        }
    }
#else
    LOG_WARNING("File dialogs not supported on this platform");
#endif
}

void EditorCore::_saveEditorState() {
    try {
        nlohmann::json state;
        state["lastScenePath"] = m_currentScenePath;
        state["lastSceneName"] = m_currentSceneName;

        std::ofstream file("editor_state.json");
        if (file.is_open()) {
            file << state.dump(4);
            file.close();
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to save editor state: " << e.what());
    }
}

void EditorCore::_loadEditorState() {
    try {
        if (!std::filesystem::exists("editor_state.json"))
            return;

        std::ifstream file("editor_state.json");
        if (!file.is_open())
            return;

        nlohmann::json state;
        file >> state;
        file.close();

        if (state.contains("lastScenePath") && !state["lastScenePath"].get<std::string>().empty()) {
            std::string path = state["lastScenePath"];
            if (std::filesystem::exists(path)) {
                if (_loadSceneFromPath(path)) {
                    m_currentScenePath = path;
                    m_currentSceneName = state.value("lastSceneName", "Untitled");
                    LOG_INFO("Restored last session: " << m_currentSceneName);
                }
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to load editor state: " << e.what());
    }
}

bool EditorCore::_saveActiveScene(const std::string& path) {
    if (!Engine::CORE) {
        LOG_ERROR("Engine CORE is null; cannot save scene.");
        return false;
    }

    auto& sm = Engine::CORE->GetSceneManager();
    size_t idx = sm.GetActiveIndex();

    if (!sm.GetActive()) {
        size_t newIdx = sm.AddScene(new Scenes::Scene());
        sm.SetActiveImmediate(newIdx);
        idx = newIdx;
    }

    return sm.SaveScene(idx, path, m_currentSceneName, "1.0");
}

bool EditorCore::_loadSceneFromPath(const std::string& path) {
    if (!Engine::CORE) {
        LOG_ERROR("Engine CORE is null; cannot load scene.");
        return false;
    }

    auto& sm = Engine::CORE->GetSceneManager();
    size_t idx = sm.GetActiveIndex();

    if (!sm.GetActive()) {
        size_t newIdx = sm.AddScene(new Scenes::Scene());
        sm.SetActiveImmediate(newIdx);
        idx = newIdx;
    }

    bool ok = sm.LoadScene(idx, path);
    if (ok) {
        m_selectedEntityId = 0;
        _invalidateCache();

        if (auto* overlay = Services::OverlayService::Get()) {
            auto* active = sm.GetActive();
            if (active) overlay->SetWorld(&active->GetWorld());
        }
    }
    return ok;
}