/* Start Header *****************************************************************/
/*!
\file   ViewportPanel.cpp
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implements the ViewportPanel class for core editor functionality and entity management.
Handles the main menu, viewport rendering, and entity operations.
*/
/* End Header *******************************************************************/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include "../editor/Viewport.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "services/Input.h"
#include <imgui.h>
#include <algorithm>
#include "services/OverlayService.h"
#include "core/Application.h"
#include "scene/Scene.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "ecs/systems/RendererSystem.h"  
#include "graphics/RenderGraph.hpp"
#include "services/Time.h"
#include "services/WindowManager.h"
// Messaging for editor warnings
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include <unordered_map>

static constexpr const char* LEVEL_DIR = "assets/levels/";
static constexpr const char* SCENE_DIR = "assets/scenes/";
static constexpr const char* SCENE_TEMPLATE_DIR = "assets/scenes/templates/";

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
void Viewport::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;

    // Create and initialize a renderer system for the editor viewport
    if (m_world && !m_rendererSystem) {
        m_rendererSystem = std::make_shared<ECS::RendererSystem>();
        m_rendererSystem->Initialize(*m_world);
        m_rendererSystem->BindWorld(*m_world);
        // Level Editor: start with the EditorCamera active in the viewport,
        // and lock it so 'C' toggle is disabled
        m_rendererSystem->ForceUseEditorCamera(true);
        m_rendererSystem->SetEditorCameraLocked(true);
    }
    // Camera warnings removed: editor always uses editor camera
}

void Viewport::SetWorld(ECS::World* world) {
    m_world = world;
    if (m_rendererSystem && world) {
        m_rendererSystem->BindWorld(*world);
    }
}


// -------------------------------------------------------------------------
// Update
// -------------------------------------------------------------------------
void Viewport::HandleInWorldInteraction() {
    if (!HasValidWorld()) return;

    // Keep selection in sync with renderer picking only when hovering the viewport
    if (m_rendererSystem && m_isViewportHovered) {
        m_selectedEntityId = m_rendererSystem->GetSelectedEntityID();
    }
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------
void Viewport::ShowEditorWindows() {
    _handleKeyboardShortcuts();

    m_uiScale = std::clamp(m_uiScale, 0.75f, 2.0f);
    ImGui::GetIO().FontGlobalScale = m_uiScale;

    _renderMainMenu();
    _renderViewport();
}

// -------------------------------------------------------------------------
// Keyboard Shortcuts
// -------------------------------------------------------------------------
void Viewport::_handleKeyboardShortcuts() {
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
        // Remove quick Save (Ctrl+S). Only allow Save As (Ctrl+Shift+S).
        if (Input::IsKeyPressed(KEY_S) && shiftDown) {
            _saveSceneAsDialog(false);
        }
    }
}

// -------------------------------------------------------------------------
// Main Menu
// -------------------------------------------------------------------------
void Viewport::_renderMainMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                _createNewScene();
            }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
                _openSceneDialog();
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                _saveSceneAsDialog(false);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                if (Engine::CORE) {
                    Engine::CORE->Close();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            // UI Scale controls
            ImGui::PushFont(m_boldFont);
            ImGui::Text("UI Scale: %.2f", m_uiScale);
            ImGui::PopFont();
            if (ImGui::MenuItem("Zoom In", "Ctrl++")) {
                m_uiScale += 0.10f;
            }
            if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) {
                m_uiScale -= 0.10f;
            }
            if (ImGui::MenuItem("Reset Scale")) {
                m_uiScale = 1.0f;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

// -------------------------------------------------------------------------
// Viewport
// -------------------------------------------------------------------------
void Viewport::_renderViewport() {
    ImGui::Begin("Viewport");

    m_isViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

    if (m_rendererSystem) {
        auto size = ImGui::GetContentRegionAvail();
        auto pos = ImGui::GetCursorScreenPos();

        if (m_world) {
            m_rendererSystem->Update(*m_world, Time::DeltaTime());
        }

        auto* rg = m_rendererSystem->GetRenderGraph();
        if (rg) {
            ResourceAccessor acc(rg);
            uint32_t textureId = static_cast<uint32_t>(acc.GetTexture("HDR"));
            if (textureId > 0) {
                ImGui::Image((void*)(intptr_t)textureId, size, ImVec2(0, 1), ImVec2(1, 0));
            }
        }
    }
    else {
        ImGui::TextDisabled("No renderer available");
    }

    ImGui::End();
}

// -------------------------------------------------------------------------
// Entity Operations
// -------------------------------------------------------------------------
// Add a new entity to the scene
void Viewport::AddEntity(const std::string& name, EntityId parentId) {
    if (!HasValidWorld()) return;

    auto entity = m_world->Create();
    ECS::Components::Name nm{};
    strncpy_s(nm.Value, name.c_str(), sizeof(nm.Value) - 1);
    nm.Value[sizeof(nm.Value) - 1] = '\0';
    m_world->Set<ECS::Components::Name>(entity, nm);
    m_world->Set<ECS::Components::LocalTransform>(entity, ECS::Components::LocalTransform());

    if (parentId != ECS::Entity::NPOS32) {
        ECS::Entity parent = m_world->Resolve(parentId);
        if (!parent.IsNull() && m_world->IsAlive(parent)) {
            m_world->Set<ECS::Parent>(entity, ECS::Parent{ parent });
        }
    }

    _invalidateCache();
    LOG_INFO("Created entity: " << name);
}

// Reparent an entity
void Viewport::ReparentEntity(EntityId childId, EntityId newParentId) {
    if (!HasValidWorld()) return;

    ECS::Entity child = m_world->Resolve(childId);
    if (child.IsNull() || !m_world->IsAlive(child)) return;

    // Remove existing parent
    if (m_world->Has<ECS::Parent>(child)) {
        m_world->Remove<ECS::Parent>(child);
    }

    // Set new parent if not root
    if (newParentId != ECS::Entity::NPOS32) {
        ECS::Entity newParent = m_world->Resolve(newParentId);
        if (!newParent.IsNull() && m_world->IsAlive(newParent)) {
            // Prevent parenting to self or descendants
            bool isDescendant = false;
            EntityId checkId = newParentId;
            while (checkId != ECS::Entity::NPOS32) {
                if (checkId == childId) {
                    isDescendant = true;
                    break;
                }
                ECS::Entity checkEntity = m_world->Resolve(checkId);
                if (m_world->Has<ECS::Parent>(checkEntity)) {
                    const auto& p = m_world->Get<ECS::Parent>(checkEntity);
                    checkId = p.ParentEntity.Index;
                }
                else {
                    break;
                }
            }

            if (!isDescendant) {
                m_world->Set<ECS::Parent>(child, ECS::Parent{ newParent });
            }
        }
    }

    _invalidateCache();
}

// Remove an entity
void Viewport::RemoveEntity(EntityId id, bool recursive) {
    if (!HasValidWorld()) return;

    ECS::Entity entity = m_world->Resolve(id);
    if (entity.IsNull() || !m_world->IsAlive(entity)) return;

    if (recursive) {
        std::vector<EntityId> toDelete;
        toDelete.push_back(id);

        // Collect all children recursively
        for (size_t i = 0; i < toDelete.size(); ++i) {
            EntityId parentId = toDelete[i];
            m_world->Each<ECS::Parent>([&](ECS::Entity e, const ECS::Parent& p) {
                if (p.ParentEntity.Index == parentId) {
                    toDelete.push_back(e.Index);
                }
                });
        }

        // Delete all collected entities
        for (auto entityId : toDelete) {
            ECS::Entity e = m_world->Resolve(entityId);
            if (!e.IsNull() && m_world->IsAlive(e)) {
                m_world->Destroy(e);
            }
        }
    }
    else {
        m_world->Destroy(entity);
    }

    _invalidateCache();
}

// Clone an entity recursively with all its children
void Viewport::CloneEntity(EntityId id) {
    if (!HasValidWorld()) return;

    ECS::Entity entity = m_world->Resolve(id);
    if (entity.IsNull() || !m_world->IsAlive(entity)) return;

    // Map from original entity ID to cloned entity
    std::unordered_map<EntityId, ECS::Entity> cloneMap;

    // Helper function to recursively clone entity and its children
    std::function<ECS::Entity(EntityId, EntityId)> cloneRecursive = [&](EntityId entityId, EntityId newParentId) -> ECS::Entity {
        ECS::Entity original = m_world->Resolve(entityId);
        if (original.IsNull() || !m_world->IsAlive(original)) {
            return ECS::NULL_ENTITY;
        }

        // Serialize and deserialize to clone
        auto entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, original);
        auto clone = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);

        // Update name with (Clone) suffix
        if (m_world->Has<ECS::Components::Name>(clone)) {
            auto& name = m_world->Get<ECS::Components::Name>(clone);
            std::string newName = std::string(name.Value) + " (Clone)";
            strncpy_s(name.Value, newName.c_str(), sizeof(name.Value) - 1);
            name.Value[sizeof(name.Value) - 1] = '\0';
        }

        // Set parent relationship
        if (newParentId != ECS::Entity::NPOS32) {
            ECS::Entity newParent = m_world->Resolve(newParentId);
            if (!newParent.IsNull() && m_world->IsAlive(newParent)) {
                m_world->Set<ECS::Parent>(clone, ECS::Parent{ newParent });
            }
        }
        else {
            // Remove parent component if cloning as root
            if (m_world->Has<ECS::Parent>(clone)) {
                m_world->Remove<ECS::Parent>(clone);
            }
        }

        // Store mapping
        cloneMap[entityId] = clone;

        // Find and clone all children
        std::vector<EntityId> children;
        m_world->Each<ECS::Parent>([&](ECS::Entity e, const ECS::Parent& p) {
            if (p.ParentEntity.Index == entityId) {
                children.push_back(e.Index);
            }
            });

        // Recursively clone children with this clone as their parent
        for (auto childId : children) {
            cloneRecursive(childId, clone.Index);
        }

        return clone;
        };

    // Get the parent of the original entity (if any)
    EntityId originalParentId = ECS::Entity::NPOS32;
    if (m_world->Has<ECS::Parent>(entity)) {
        const auto& parent = m_world->Get<ECS::Parent>(entity);
        originalParentId = parent.ParentEntity.Index;
    }

    // Clone the entity hierarchy
    cloneRecursive(id, originalParentId);

    _invalidateCache();
    LOG_INFO("Cloned entity " << id << " with all children");
}

// Clear all entities
void Viewport::ClearAllEntities() {
    if (!HasValidWorld()) return;

    std::vector<ECS::Entity> allEntities;
    m_world->Each([&](ECS::Entity e) {
        // Keep editor camera
        if (m_world->Has<ECS::Components::Name>(e)) {
            const auto& name = m_world->Get<ECS::Components::Name>(e);
            if (std::strcmp(name.Value, "EditorCamera") == 0 ||
                std::strcmp(name.Value, "Editor Camera") == 0) {
                return;
            }
        }
        allEntities.push_back(e);
        });

    for (const auto& e : allEntities) {
        m_world->Destroy(e);
    }

    _invalidateCache();
    LOG_INFO("Cleared all entities");
}

// Focus camera on entity
void Viewport::FocusOnEntity(EntityId id) {
    if (!HasValidWorld() || !m_rendererSystem) return;

    ECS::Entity entity = m_world->Resolve(id);
    if (entity.IsNull() || !m_world->IsAlive(entity)) return;

    if (m_world->Has<ECS::Components::LocalTransform>(entity)) {
        const auto& transform = m_world->Get<ECS::Components::LocalTransform>(entity);
        // Move editor camera to entity position
        // (Implementation depends on camera system)
        LOG_INFO("Focusing on entity " << id);
    }
}

// -------------------------------------------------------------------------
// Scene Management
// -------------------------------------------------------------------------
void Viewport::_createNewScene() {
    if (!Engine::CORE) return;

    auto& sm = Engine::CORE->GetSceneManager();
    auto newScene = std::make_unique<Scenes::Scene>();
    newScene->SetName("New Scene");
    size_t idx = sm.AddScene(newScene.release());
    sm.SetActive(idx);

    LOG_INFO("Created new scene");
}

void Viewport::_openSceneDialog() {
#ifdef _WIN32
    OPENFILENAMEA ofn = {};
    char szFile[260] = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = glfwGetWin32Window(WindowManager::GetMainWindow()->Handle());
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = SCENE_DIR;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        _openScene(szFile);
    }
#endif
}

void Viewport::_openScene(const std::string& path) {
    if (!Engine::CORE) return;

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open scene file: " << path);
            return;
        }

        nlohmann::json sceneJson;
        file >> sceneJson;
        file.close();

        auto& sm = Engine::CORE->GetSceneManager();
        auto newScene = std::make_unique<Scenes::Scene>();
        newScene->SetName("Loaded Scene");

        // Load entities
        if (sceneJson.contains("Entities")) {
            for (auto& entityJson : sceneJson["Entities"]) {
                Serialization::EntitySerializer::DeserializeEntity(newScene->GetWorld(), entityJson);
            }
        }

        size_t idx = sm.AddScene(newScene.release());
        sm.SetActive(idx);
        LOG_INFO("Opened scene: " << path);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to open scene: " << e.what());
    }
}

void Viewport::_saveScene() {
    // Save scene to current path (not implemented - use Save As)
    _saveSceneAsDialog(false);
}

void Viewport::_saveSceneAsDialog(bool isTemplate) {
#ifdef _WIN32
    OPENFILENAMEA ofn = {};
    char szFile[260] = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = glfwGetWin32Window(WindowManager::GetMainWindow()->Handle());
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = isTemplate ? SCENE_TEMPLATE_DIR : SCENE_DIR;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        std::string savePath = szFile;
        if (savePath.find(".scene") == std::string::npos) {
            savePath += ".scene";
        }
        _saveSceneToFile(savePath);
    }
#endif
}

void Viewport::_saveSceneToFile(const std::string& path) {
    if (!HasValidWorld()) return;

    try {
        nlohmann::json sceneJson;
        sceneJson["Entities"] = nlohmann::json::array();

        m_world->Each([&](ECS::Entity entity) {
            // Skip editor camera
            if (m_world->Has<ECS::Components::Name>(entity)) {
                const auto& name = m_world->Get<ECS::Components::Name>(entity);
                if (std::strcmp(name.Value, "EditorCamera") == 0 ||
                    std::strcmp(name.Value, "Editor Camera") == 0) {
                    return;
                }
            }

            auto entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
            sceneJson["Entities"].push_back(entityJson);
            });

        std::ofstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("Cannot write to scene file: " << path);
            return;
        }

        file << sceneJson.dump(2);
        file.close();

        LOG_INFO("Saved scene: " << path);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to save scene: " << e.what());
    }
}

// -------------------------------------------------------------------------
// Helper Methods
// -------------------------------------------------------------------------
void Viewport::_invalidateCache() {
    // Invalidate any cached entity data
}
