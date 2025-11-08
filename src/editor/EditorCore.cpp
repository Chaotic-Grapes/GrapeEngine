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

static constexpr const char* LEVEL_DIR = "assets/levels/";
static constexpr const char* SCENE_DIR = "assets/scenes/";
static constexpr const char* SCENE_TEMPLATE_DIR = "assets/scenes/templates/";

void EditorCore::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    // Editor state persistence disabled: do not load last session

    // Create and initialize a renderer system for the editor viewport
    if (m_world && !m_rendererSystem) {
        m_rendererSystem = std::make_shared<ECS::RendererSystem>();
        m_rendererSystem->Initialize(*m_world);
        m_rendererSystem->BindWorld(*m_world);
    }

    // Subscribe to DebugMessage to surface transient warnings in the viewport toolbar
    m_debugMsgSubscription = Messaging::MessageSystem::Subscribe<Messaging::DebugMessage>(
        [this](const Messaging::DebugMessage& msg) {
            if (msg.Source == std::string("RendererSystem") &&
                msg.LogLevel == Messaging::DebugMessage::Level::Warning) {
                // Show warning for a short duration
                m_cameraToggleWarning = msg.Message;
                m_cameraWarningExpiry = Time::ElapsedTime() + 3.0;
            }
        }
    );
}

void EditorCore::HandleInWorldInteraction() {
    if (!HasValidWorld()) return;

    static bool isDragging = false;
    static double lastMouseX = 0.0;
    static double lastMouseY = 0.0;

    // Keep selection in sync with renderer picking only when hovering the viewport
    if (m_rendererSystem && m_isViewportHovered) {
        m_selectedEntityId = m_rendererSystem->GetSelectedEntityID();
    }

    // Drag when hovering viewport and holding LMB on a selected entity
    if (m_isViewportHovered && Input::IsMouseDown(GLFW_MOUSE_BUTTON_LEFT) && m_selectedEntityId != 0) {
        double xPos, yPos;
        Input::GetMousePosition(xPos, yPos);

        if (!isDragging) {
            isDragging = true;
            lastMouseX = xPos;
            lastMouseY = yPos;
        } else {
            const double dx = xPos - lastMouseX;
            const double dy = yPos - lastMouseY;
            lastMouseX = xPos;
            lastMouseY = yPos;

            const auto window = WindowManager::GetMainWindow();
            if (window && m_rendererSystem) {
                const float screenW = static_cast<float>(window->Width());
                const float screenH = static_cast<float>(window->Height());
                const float aspect = screenW / screenH;
                const float worldHeight = m_rendererSystem->GetCameraOrthoSize();
                const float worldWidth = worldHeight * aspect;

                const float worldDeltaX = static_cast<float>(-(dx / screenW) * worldWidth);
                const float worldDeltaY = static_cast<float>((dy / screenH) * worldHeight);

                ECS::Entity selectedEntity{ m_selectedEntityId, 0 };
                if (m_world->IsAlive(selectedEntity) && m_world->Has<ECS::Components::LocalTransform>(selectedEntity)) {
                    auto& transform = m_world->Get<ECS::Components::LocalTransform>(selectedEntity);
                    transform.Position.X += worldDeltaX;
                    transform.Position.Y += worldDeltaY;
                }
            }
        }
    } else {
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
        // Remove quick Save (Ctrl+S). Only allow Save As (Ctrl+Shift+S).
        if (Input::IsKeyPressed(KEY_S) && shiftDown) {
            _saveSceneAsDialog(false);
        }
    }

    m_uiScale = std::clamp(m_uiScale, 0.75f, 2.0f);
    ImGui::GetIO().FontGlobalScale = m_uiScale;
    _showMainMenu();

    // Keep renderer in sync before drawing viewport
    if (m_world && m_rendererSystem) {
        m_rendererSystem->Update(*m_world, static_cast<float>(Time::DeltaTime()));
    }
    _showViewport();
}

void EditorCore::_showMainMenu() {
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

            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                _saveSceneAsDialog(false);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void EditorCore::_showViewport() {
    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Viewport");

    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        auto* activeScene = sm.GetActive();

        if (activeScene) {
            // Toolbar: basic viewport controls and camera toggle hint
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
                ImGui::BeginChild("ViewportToolbar", ImVec2(-1, 28), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                // Runtime indicator: show which camera is driving the viewport
                if (m_rendererSystem) {
                    const bool usingEditor = m_rendererSystem->IsUsingEditorCamera();
                    ImVec4 camColor = usingEditor ? ImVec4(0.40f, 0.70f, 1.00f, 1.0f) : ImVec4(0.25f, 0.85f, 0.45f, 1.0f);
                    const char* camLabel = usingEditor ? "Camera: Editor" : "Camera: Scene";
                    ImGui::TextColored(camColor, "%s", camLabel);
                    ImGui::SameLine();
                }
                ImGui::TextDisabled("Press C to toggle Editor/Scene camera | LMB: Pan | RMB: Orbit | Scroll: Zoom");
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
            }

            // Small floating popup for transient warnings (e.g., no active Camera3D)
            if (m_cameraWarningExpiry > 0.0) {
                if (Time::ElapsedTime() >= m_cameraWarningExpiry) {
                    m_cameraWarningExpiry = 0.0;
                    m_cameraToggleWarning.clear();
                } else {
                    // Center the popup inside the viewport window
                    ImVec2 winPos = ImGui::GetWindowPos();
                    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
                    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
                    ImVec2 center = ImVec2(
                        winPos.x + (contentMin.x + contentMax.x) * 0.5f,
                        winPos.y + (contentMin.y + contentMax.y) * 0.5f
                    );

                    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowBgAlpha(0.95f);
                    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;
                    ImGui::Begin("Camera Warning##Popup", nullptr, flags);
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", m_cameraToggleWarning.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Dismiss##CameraWarn")) {
                        m_cameraWarningExpiry = 0.0;
                        m_cameraToggleWarning.clear();
                    }
                    ImGui::End();
                }
            }

            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            GLuint sceneTexture = 0;
            if (m_rendererSystem && m_rendererSystem->GetRenderGraph()) {
                auto* hdrFbo = m_rendererSystem->GetRenderGraph()->GetFramebuffer("HDR");
                if (hdrFbo) sceneTexture = hdrFbo->GetColorTexture(0);
            }

            if (sceneTexture != 0) {
                ImGui::Image((void*)(intptr_t)sceneTexture, viewportSize, ImVec2(0, 1), ImVec2(1, 0));

                // Track hover and absolute rect for picking/dragging
                m_isViewportHovered = ImGui::IsItemHovered();
                if (m_rendererSystem) {
                    const ImVec2 min = ImGui::GetItemRectMin();
                    const ImVec2 max = ImGui::GetItemRectMax();
                    const float x = min.x;
                    const float y = min.y;
                    const float w = max.x - min.x;
                    const float h = max.y - min.y;
                }
            } else {
                ImGui::TextDisabled("Renderer not initialized or HDR texture missing");
            }

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

    // Ensure new entities participate in rendering order even if user doesn't add Layer manually
    auto& layerComp = m_world->Add<ECS::Components::Layer>(newEntity);
    layerComp.Id = 0;

    if (parentId != ECS::Entity::NPOS32) {
        ECS::Entity parent = m_world->Resolve(parentId);
        if (!parent.IsNull()) {
            m_world->Add<ECS::Parent>(newEntity, ECS::Parent{ parent });
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
        if (m_world->Has<ECS::Parent>(child)) {
            m_world->Set<ECS::Parent>(child, ECS::Parent{ newParent });
        }
        else {
            m_world->Add<ECS::Parent>(child, ECS::Parent{ newParent });
        }
    }
}

void EditorCore::RemoveEntity(EntityId id, bool recursive) {
    if (!HasValidWorld()) return;

    ECS::Entity entity = m_world->Resolve(id);
    if (!m_world->IsAlive(entity)) return;

    if (recursive) {
        m_world->ForChildren(entity, [&](ECS::Entity child) {
            RemoveEntity(child.Index, true);
            });
    }

    m_world->Destroy(entity);
    // If we removed the currently selected entity, clear selection
    if (m_selectedEntityId == id) {
        m_selectedEntityId = 0;
    }
    _invalidateCache();
}

void EditorCore::CloneEntity(EntityId id) {
    if (!HasValidWorld()) return;

    ECS::Entity source = m_world->Resolve(id);
    if (source.IsNull()) return;

    std::function<nlohmann::json(ECS::Entity)> serializeHierarchy = [&](ECS::Entity entity) {
        nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

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

    // Sanitize JSON to modern schema to avoid legacy string-based Name data
    std::function<void(nlohmann::json&)> sanitizeEntityJson = [&](nlohmann::json& entityJson) {
        if (!entityJson.is_object()) return;
        if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
            for (auto& comp : entityJson["Components"]) {
                if (!comp.is_object()) continue;
                // Accept modern TypeName or legacy Type keys
                std::string typeName;
                if (comp.contains("TypeName") && comp["TypeName"].is_string()) {
                    typeName = comp["TypeName"].get<std::string>();
                } else if (comp.contains("Type") && comp["Type"].is_string()) {
                    typeName = comp["Type"].get<std::string>();
                }
                if (typeName == "ECS::Components::Name" || typeName == "Name") {
                    if (comp.contains("Data") && comp["Data"].is_string()) {
                        // Coerce string into object form: { "Value": "..." }
                        const std::string nameStr = comp["Data"].get<std::string>();
                        comp["Data"] = nlohmann::json{ {"Value", nameStr} };
                    }
                }
            }
        }
        if (entityJson.contains("Children") && entityJson["Children"].is_array()) {
            for (auto& child : entityJson["Children"]) {
                sanitizeEntityJson(child);
            }
        }
    };
    sanitizeEntityJson(sourceHierarchy);

    std::function<ECS::Entity(const nlohmann::json&, ECS::Entity)> deserializeHierarchy =
        [&](const nlohmann::json& entityJson, ECS::Entity parent) -> ECS::Entity {
        ECS::Entity clonedEntity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);

        if (!parent.IsNull()) {
            m_world->Add<ECS::Parent>(clonedEntity, ECS::Parent{ parent });
        }

        if (entityJson.contains("Children") && entityJson["Children"].is_array()) {
            for (const auto& childJson : entityJson["Children"]) {
                deserializeHierarchy(childJson, clonedEntity);
            }
        }

        return clonedEntity;
        };

    ECS::Entity originalParent = m_world->ParentOf(source);
    ECS::Entity clone = deserializeHierarchy(sourceHierarchy, originalParent);

    LOG_INFO("Cloned entity " << id << " with hierarchy");
}

void EditorCore::ClearAllEntities() {
    if (!HasValidWorld()) return;

    // PHASE 1: Collect all entities first to avoid iterator invalidation
    std::vector<ECS::Entity> allEntities;
    allEntities.reserve(1000); // Reserve space to avoid reallocation

    m_world->Each([&](ECS::Entity e) {
        allEntities.push_back(e);
        });

    // PHASE 2: Now safely destroy all entities outside of the iteration
    for (const auto& entity : allEntities) {
        if (m_world->IsAlive(entity)) {
            m_world->Destroy(entity);
        }
    }

    m_selectedEntityId = 0;
    _invalidateCache();

    // Rebind overlay world reference to ensure editor panels refresh cleanly
    if (auto* overlay = Services::OverlayService::Get()) {
        overlay->SetWorld(m_world);
    }
}

bool EditorCore::HasValidWorld() const {
    return m_world != nullptr;
}

void EditorCore::SetWorld(ECS::World* world) {
    m_world = world;
    m_selectedEntityId = 0;
    _invalidateCache();

    // Reinitialize renderer system to bind to the new world
    if (m_world) {
        if (!m_rendererSystem) {
            m_rendererSystem = std::make_shared<ECS::RendererSystem>();
            // Initialize GL resources once
            m_rendererSystem->Initialize(*m_world);
        }
        // Bind the current world every time (creates/activates editor camera exactly once)
        m_rendererSystem->BindWorld(*m_world);
    } else {
        m_rendererSystem.reset();
    }
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

ECS::Entity EditorCore::_createGameEntity(const std::string& name) {
    if (!HasValidWorld()) return ECS::Entity{ ECS::Entity::NPOS32, 0 };

    ECS::Entity newEntity = m_world->Create();

    auto& nameComp = m_world->Add<ECS::Components::Name>(newEntity);
    std::strncpy(nameComp.Value, name.c_str(), sizeof(nameComp.Value) - 1);
    nameComp.Value[sizeof(nameComp.Value) - 1] = '\0';

    auto& transform = m_world->Add<ECS::Components::LocalTransform>(newEntity);
    transform.Position = { 0.0f, 0.0f, 0.0f };
    transform.Scale = { 1.0f, 1.0f, 1.0f };
    transform.Rotation = Quaternion::Identity();

    return newEntity;
}

std::vector<EntityId> EditorCore::_getChildren(EntityId parentId) const {
    std::vector<EntityId> children;

    if (!HasValidWorld()) return children;

    ECS::Entity parentEntity = m_world->Resolve(parentId);
    if (parentEntity.IsNull()) return children;

    m_world->ForChildren(parentEntity, [&](ECS::Entity child) {
        children.push_back(child.Index);
        });

    return children;
}

void EditorCore::_createNewScene() {
    LOG_INFO("New Scene requested");

    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        size_t newIdx = sm.AddScene(new Scenes::Scene());
        sm.SetActiveImmediate(newIdx);
        // Do not bind world here; LevelEditor::Update will detect active change
        // and call LevelEditor::SetWorld -> EditorCore::SetWorld once.

        if (auto* overlay = Services::OverlayService::Get()) {
            auto* active = sm.GetActive();
            if (active) overlay->SetWorld(&active->GetWorld());
        }

        // Do not create a default scene camera; rely on the internal Editor Camera.
    }

    m_currentScenePath.clear();
    m_currentSceneName = "Untitled";

    // Do not change camera mode on new scene; keep previous behavior
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
            LOG_INFO("Loaded scene: " << m_currentSceneName);
        }
    }
#else
    LOG_WARNING("File dialogs not supported on this platform");
#endif
}

void EditorCore::_saveScene() {
    if (m_currentScenePath.empty()) {
        // Save to default path without opening a dialog
        std::string defaultName = m_currentSceneName.empty() ? "Untitled" : m_currentSceneName;
        std::filesystem::path dir = std::filesystem::path(SCENE_DIR);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::path path = dir / (defaultName + ".json");

        if (_saveActiveScene(path.string())) {
            m_currentScenePath = path.string();
            m_currentSceneName = defaultName;
            LOG_INFO("Saved scene: " << m_currentSceneName);
        }
    }
    else {
        if (_saveActiveScene(m_currentScenePath)) {
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
        // Do not bind world here; LevelEditor::Update will detect active change
        // and call LevelEditor::SetWorld -> EditorCore::SetWorld once.

        // Keep camera mode unchanged after loading
    }
    return ok;
}