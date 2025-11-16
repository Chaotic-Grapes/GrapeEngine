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
