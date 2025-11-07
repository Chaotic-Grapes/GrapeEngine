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
- EditorCore = Entity/Level management (Model/Controller layer)
- Centralized entity operations: add, remove, clone, clear, reparent
- Handles in-world entity picking/dragging in viewport
- Future: Level save/load management
*/
/* End Header *******************************************************************/

// Windows-specific includes for file dialogs
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

// Directory for level files (used by commented out save/load)
static constexpr const char* LEVEL_DIR = "assets/levels/";
static constexpr const char* SCENE_DIR = "assets/scenes/";
static constexpr const char* SCENE_TEMPLATE_DIR = "assets/scenes/templates/";

// No global flags; viewport hint is purely based on scene emptiness

// Set up fonts and world reference
void EditorCore::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;

    // Load last session if available
    _loadEditorState();
}

// Handle mouse based entity selection and dragging in viewport
// Uses circle collider radius for picking hitbox
void EditorCore::HandleInWorldInteraction() {
    if (!HasValidWorld()) return;

    // Track if we are currently dragging an entity
    static bool isDragging = false;

    // Don't interfere if mouse is over ImGui windows
    if (ImGui::GetIO().WantCaptureMouse) return;

    // Get current mouse position in world space
    double xPos, yPos;
    Input::GetMousePosition(xPos, yPos);
    Vector2D mouseWorldPos(static_cast<float>(xPos), static_cast<float>(yPos));

    // PICKING: Start selecting/dragging on left click
    if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        EntityId newSelection = 0;

        // Collect all entities with both LocalTransform and CircleCollider2D
        std::vector<ECS::Entity> pickableEntities;
        m_world->Each<ECS::Components::LocalTransform, ECS::Components::CircleCollider2D>(
            [&](ECS::Entity e, ECS::Components::LocalTransform&, ECS::Components::CircleCollider2D&) {
                pickableEntities.push_back(e);
            }
        );

        // Iterate entities in reverse to pick topmost rendered object first
        for (auto it = pickableEntities.rbegin(); it != pickableEntities.rend(); it++) {
            ECS::Entity entity = *it;

            // Get transform & collider
            const auto& transform = m_world->Get<ECS::Components::LocalTransform>(entity);
            const auto& collider = m_world->Get<ECS::Components::CircleCollider2D>(entity);

            // Simple circle based picking: is mouse inside radius
            float distance = MathHelper::Distance(
                Vector2D(transform.Position.X, transform.Position.Y),
                mouseWorldPos
            );

            if (distance <= collider.Radius) {
                newSelection = entity.Index;
                isDragging = true;
                break; // Found topmost object
            }
        }

        // Update selection if changed
        if (m_selectedEntityId != newSelection) {
            m_selectedEntityId = newSelection;
            _invalidateCache();
        }
    }

    // DRAGGING: Move entity to follow mouse while button held
    if (isDragging && m_selectedEntityId != 0 && Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        ECS::Entity selectedEntity{ m_selectedEntityId, 0 };

        if (m_world->IsAlive(selectedEntity) && m_world->Has<ECS::Components::LocalTransform>(selectedEntity)) {
            // Directly set position to mouse world position
            auto& transform = m_world->Get<ECS::Components::LocalTransform>(selectedEntity);
            transform.Position.X = mouseWorldPos.X;
            transform.Position.Y = mouseWorldPos.Y;
        }
    }

    // DROP: Stop dragging when button released
    if (isDragging && !Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        isDragging = false;
    }
}

// Render main menu bar (currently just File menu)
void EditorCore::ShowEditorWindows() {
    // Keyboard shortcuts: Ctrl+= (Zoom In), Ctrl+- (Zoom Out)
    bool ctrlDown = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
    bool shiftDown = Input::IsKeyDown(KEY_LEFT_SHIFT) || Input::IsKeyDown(KEY_RIGHT_SHIFT);
    if (ctrlDown) {
        // Support both main keyboard and numpad variants
        if (Input::IsKeyPressed(KEY_EQUAL) || Input::IsKeyPressed(GLFW_KEY_KP_ADD)) {
            m_uiScale += 0.10f;
        }
        if (Input::IsKeyPressed(KEY_MINUS) || Input::IsKeyPressed(GLFW_KEY_KP_SUBTRACT)) {
            m_uiScale -= 0.10f;
        }

        // File actions
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

    // Clamp and apply global UI scale for this frame
    m_uiScale = std::clamp(m_uiScale, 0.75f, 2.0f);
    ImGui::GetIO().FontGlobalScale = m_uiScale;
    _showMainMenu();
    _showViewport();
}

// Render File menu with save/load options (currently disabled)
void EditorCore::_showMainMenu() {
    // Black background for menu bar
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
    // Remove border from menu bar only
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::BeginMainMenuBar()) {
        // View menu (placed before File) with compact scaling controls
        if (ImGui::BeginMenu("View")) {
            ImGui::Text("UI Scale: %.0f%%", m_uiScale * 100.0f);
            ImGui::Separator();

            // Quick actions
            if (ImGui::MenuItem("Zoom In", "Ctrl++")) {
                m_uiScale += 0.10f;
            }
            if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) {
                m_uiScale -= 0.10f;
            }

            // Presets submenu (restored)
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

            // Reset to default
            if (ImGui::MenuItem("Reset to 135%")) {
                m_uiScale = 1.35f;
            }

            // Clamp to safe bounds
            m_uiScale = std::clamp(m_uiScale, 0.75f, 2.0f);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("File")) {
            // New / Open
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                _createNewScene();
            }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
                _openSceneDialog();
            }

            ImGui::Separator();

            // Save options
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                _saveScene();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                _saveSceneAsDialog(false);
            }
            if (ImGui::MenuItem("Save As Scene Template")) {
                _saveSceneAsDialog(true);
            }

            ImGui::Separator();

            // Exit option
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // TODO: Handle application exit
            }

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// Render a docked Viewport window
// Scene rendering happens behind via dockspace passthrough
void EditorCore::_showViewport() {
    // Configure window flags: allow mouse interaction but disable scrolling
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;

    // Begin the Viewport window (will be docked in main dockspace)
    if (ImGui::Begin("Viewport", nullptr, flags)) {
        // Get available rendering space inside the window
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        // Determine if we have an attached scene (only checks SceneManager active scene)
        bool sceneAttached = false;
        if (Engine::CORE) {
            auto& sm = Engine::CORE->GetSceneManager();
            sceneAttached = sm.GetActive() != nullptr;
        }

        // Show toolbar only when a scene is attached
        if (sceneAttached) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f)); // Set dark gray bg
            ImGui::BeginChild("ViewportToolbar", ImVec2(avail.x, 28.0f), false);        // Create toolbar child window (full width, 28 px)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));    // Set light gray text color for instructions
            ImGui::Text("Camera: Press 'C' to toggle Editor Camera | LMB to pan | RMB to orbit | Scroll to zoom");
            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        // Create main viewport area (fills remaining space below toolbar)
        // Scene is rendered to this region via the dockspace central node
        const float viewportHeight = sceneAttached ? (avail.y - 28.0f) : avail.y;
        ImGui::BeginChild("ViewportArea", ImVec2(avail.x, viewportHeight), false);

        // When no scene is attached, show guidance
        const bool noScene = !sceneAttached;
        if (noScene) {
            ImGui::TextDisabled("No scene attached");
            ImGui::Dummy(ImVec2(0, 3));
            ImGui::TextDisabled("Use File > New Scene to create an empty scene");
        }

        ImGui::EndChild();
    }
    ImGui::End();
}

// Check if world pointer is valid before doing operations
bool EditorCore::HasValidWorld() const {
    return m_world != nullptr;
}

// Update the world reference and clear any stale UI state
void EditorCore::SetWorld(ECS::World* world) {
    m_world = world;
    m_selectedEntityId = 0;
    _invalidateCache();
}

// Create new entity with given name at random screen position
void EditorCore::AddEntity(const std::string & name, EntityId parentId) {
    // Validate name and world
    if (name.empty() || name.length() > MAX_OBJECT_NAME_LENGTH || !HasValidWorld())
        return;

    // Create entity with default components
    auto entity = _createGameEntity(name);

    // Set parent if specified (use Parent component)
    if (parentId != 0) {
        ECS::Entity parentEntity{ parentId, 0 };
        if (m_world->IsAlive(parentEntity)) {
            m_world->Add<ECS::Parent>(entity, parentEntity);
        }
    }

    // Random position so entities don't spawn on top of each other
    if (m_world->Has<ECS::Components::LocalTransform>(entity)) {
        auto& transform = m_world->Get<ECS::Components::LocalTransform>(entity);
        transform.Position.X = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowWidth()));
        transform.Position.Y = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowHeight()));
    }

    // Clear cached UI labels for new entity
    _invalidateCache();
}

// Delete entity by ID from world (optionally recursive for children)
void EditorCore::RemoveEntity(const EntityId id, bool recursive) {
    if (!HasValidWorld()) return;

    ECS::Entity entity{ id, 0 };
    if (!m_world->IsAlive(entity)) return;

    // If recursive, delete all children first
    if (recursive) {
        auto children = _getChildren(id);
        for (auto childId : children) {
            RemoveEntity(childId, true); // Recursively delete children
        }
    }

    m_world->Destroy(entity);
    _invalidateCache();
}

// Clone entity with slight position offset so it doesn't overlap original
void EditorCore::CloneEntity(const EntityId id) {
    if (!HasValidWorld()) return;

    ECS::Entity entity{ id, 0 };
    if (!m_world->IsAlive(entity)) return;

    // Use World's Clone method with options
    ECS::CloneOptions options;
    options.KeepParent = false;  // Don't copy parent relationship
    options.KeepName = true;     // Keep the name
    options.KeepLayer = true;    // Keep the layer

    ECS::Entity cloned = m_world->Clone(entity, options);

    // Offset so clone is visible next to original
    if (m_world->Has<ECS::Components::LocalTransform>(cloned)) {
        auto& transform = m_world->Get<ECS::Components::LocalTransform>(cloned);
        transform.Position.X += 50.0f;
        transform.Position.Y += 50.0f;
    }

    _invalidateCache();
}

// Delete all entities in world
void EditorCore::ClearAllEntities() {
    if (!HasValidWorld()) return;

    // Collect all entities first to avoid iterator invalidation
    std::vector<ECS::Entity> allEntities;
    m_world->Each([&](ECS::Entity e) {
        allEntities.push_back(e);
        });

    // Destroy them all
    for (const auto& e : allEntities) {
        m_world->Destroy(e);
    }

    _invalidateCache();
}

// Create entity with default components: Name, LocalTransform, ShapeCircle2D, CircleCollider2D
// Color is set based on entity name for quick visual identification
ECS::Entity EditorCore::_createGameEntity(const std::string & name) {
    // Create entity
    auto entity = m_world->Create();

    // Add Name component
    auto& nameComp = m_world->Add<ECS::Components::Name>(entity);
    strncpy_s(nameComp.Value, name.c_str(), sizeof(nameComp.Value) - 1);
    nameComp.Value[sizeof(nameComp.Value) - 1] = '\0';

    // Transform is required for all entities
    m_world->Add<ECS::Components::LocalTransform>(entity);

    // Visual shape for rendering (using ShapeCircle2D instead of old ShapeRenderer2D)
    auto& shapeCircle = m_world->Add<ECS::Components::ShapeCircle2D>(entity);
    shapeCircle.Radius = 35.0f;
    shapeCircle.Filled = true;

    // Color based on name for easy identification
    if (name == "Player")
        shapeCircle.Color = Color(0.0f, 0.0f, 1.0f, 1.0f); // Blue
    else if (name == "Enemy")
        shapeCircle.Color = Color(1.0f, 0.0f, 0.0f, 1.0f); // Red
    else if (name == "Collectible")
        shapeCircle.Color = Color(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
    else
        shapeCircle.Color = Color(1.0f, 1.0f, 1.0f, 1.0f); // White

    // Collider for picking and physics
    auto& collider = m_world->Add<ECS::Components::CircleCollider2D>(entity);
    collider.Radius = 35.0f;

    return entity;
}

// Clear all cached UI label strings (call when entity list changes)
void EditorCore::_invalidateCache() {
    m_cachedDeleteLabels.clear();
    m_cachedCloneLabels.clear();
}

// Change an entity's parent while preventing circular references
void EditorCore::ReparentEntity(EntityId childId, EntityId newParentId) {
    // Can't parent to self
    if (!HasValidWorld() || childId == newParentId) return;

    ECS::Entity childEntity{ childId, 0 };
    ECS::Entity newParentEntity{ newParentId, 0 };

    if (!m_world->IsAlive(childEntity)) return;
    if (newParentId != 0 && !m_world->IsAlive(newParentEntity)) return;

    // Walk up the ancestry chain of the new parent to check for circular parenting
    // If we encounter the child anywhere in the chain, we would create a loop
    EntityId checkId = newParentId;
    while (checkId != 0) {
        if (checkId == childId) {
            LOG_WARNING("Cannot create circular parent relationship");
            return;
        }

        // Move one level up in the hierarchy by following Parent component
        ECS::Entity checkEntity{ checkId, 0 };
        if (!m_world->IsAlive(checkEntity) || !m_world->Has<ECS::Parent>(checkEntity)) {
            break;
        }

        const auto& parentComp = m_world->Get<ECS::Parent>(checkEntity);
        checkId = parentComp.ParentEntity.Index;
    }

    // At this point we are sure there is no circular parenting happening
    // Update or add the Parent component
    if (newParentId == 0) {
        // Remove parent (make it a root entity)
        if (m_world->Has<ECS::Parent>(childEntity)) {
            m_world->Remove<ECS::Parent>(childEntity);
        }
    }
    else {
        // Set new parent
        if (m_world->Has<ECS::Parent>(childEntity)) {
            auto& parentComp = m_world->Get<ECS::Parent>(childEntity);
            parentComp.ParentEntity = newParentEntity;
        }
        else {
            m_world->Add<ECS::Parent>(childEntity, newParentEntity);
        }
    }

    LOG_INFO("Reparented entity " << childId << " to " << newParentId);
}

// Helper to find all entities that have this entity as their parent
// We need this for recursive deletion, otherwise we'd orphan child entities when deleting parents
// Uses World's ForChildren helper which uses the Parent component
std::vector<EntityId> EditorCore::_getChildren(EntityId parentId) const {
    std::vector<EntityId> children;
    ECS::Entity parentEntity{ parentId, 0 };

    if (!m_world->IsAlive(parentEntity)) return children;

    // Use World's ForChildren method to iterate children
    m_world->ForChildren(parentEntity, [&](ECS::Entity child) {
        children.push_back(child.Index);
        });

    return children;
}

// ImGui needs unique string IDs for every button, otherwise it gets confused about which one we clicked
// We cache these strings per entity so we're not allocating new strings every single frame
const std::string& EditorCore::_getDeleteLabel(const EntityId id) const {
    auto it = m_cachedDeleteLabels.find(id);
    if (it == m_cachedDeleteLabels.end()) {
        // The ## part is ImGui's way of hiding the ID from the visible button text
        std::string label = "Delete##" + std::to_string(id);
        it = m_cachedDeleteLabels.insert({ id, label }).first;
    }
    return it->second;
}

// We keep separate caches because the same entity could have both delete and clone buttons visible
// Caching these saves us from doing string concatenation and allocation every frame
const std::string& EditorCore::_getCloneLabel(const EntityId id) const {
    auto it = m_cachedCloneLabels.find(id);
    if (it == m_cachedCloneLabels.end()) {
        std::string label = "Clone##" + std::to_string(id);
        it = m_cachedCloneLabels.insert({ id, label }).first;
    }
    return it->second;
}

// Tracks whether each entity's tree node is expanded or collapsed in the hierarchy
// ImGui needs a persistent bool reference to remember the expand/collapse state between frames
const bool& EditorCore::_getCollapsedHeaderBool(const EntityId id) const {
    auto it = m_cachedCollapsedHeaders.find(id);
    if (it == m_cachedCollapsedHeaders.end()) {
        // We store these in a map so each entity remembers its own state independently
        it = m_cachedCollapsedHeaders.insert({ id, false }).first;
    }
    return it->second;
}

// ---------------- Scene Save/Load helpers ---------------- //

// Create a new empty scene
void EditorCore::_createNewScene() {
    LOG_INFO("New Scene requested");

    // Save current scene path for persistence
    _saveEditorState();

    // If no active scene, create and attach a new empty scene
    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        if (!sm.GetActive()) {
            size_t newIdx = sm.AddScene(new Scenes::Scene());
            sm.SetActiveImmediate(newIdx);
            // Immediately sync overlay world so panels appear without waiting a frame
            if (auto* overlay = Services::OverlayService::Get()) {
                auto* active = sm.GetActive();
                if (active) overlay->SetWorld(&active->GetWorld());
            }
        }
    }
    // Clear entities if a world exists and reset metadata
    ClearAllEntities();
    m_currentScenePath.clear();
    m_currentSceneName = "Untitled";
}

// Open file dialog to load a scene
void EditorCore::_openSceneDialog() {
#ifdef _WIN32
    char filename[512] = "";

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "Scene Files\0*.scn\0All Files\0*.*\0";
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

// Save current scene (or prompt if no path)
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

// Open file dialog to save scene as new file
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
    ofn.lpstrFilter = "Scene Files\0*.scn\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = isTemplate ? "Save Scene Template" : "Save Scene As";
    ofn.lpstrInitialDir = isTemplate ? SCENE_TEMPLATE_DIR : SCENE_DIR;
    ofn.lpstrDefExt = "scn";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        std::string path(filename);

        // Ensure directory exists
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

// Save editor state to JSON file for persistence
void EditorCore::_saveEditorState() const {
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

// Load editor state from JSON file
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

bool EditorCore::_saveActiveScene(const std::string & path) const {
    if (!Engine::CORE) {
        LOG_ERROR("Engine CORE is null; cannot save scene.");
        return false;
    }
    auto& sm = Engine::CORE->GetSceneManager();
    size_t idx = sm.GetActiveIndex();
    if (!sm.GetActive()) {
        // Create an empty scene slot and make it active immediately for saving
        size_t newIdx = sm.AddScene(new Scenes::Scene());
        sm.SetActiveImmediate(newIdx);
        idx = newIdx;
    }

    return sm.SaveScene(idx, path, m_currentSceneName, "1.0");
}

bool EditorCore::_loadSceneFromPath(const std::string & path) {
    if (!Engine::CORE) {
        LOG_ERROR("Engine CORE is null; cannot load scene.");
        return false;
    }
    auto& sm = Engine::CORE->GetSceneManager();
    size_t idx = sm.GetActiveIndex();
    if (!sm.GetActive()) {
        // Create an empty scene slot and make it active immediately for loading
        size_t newIdx = sm.AddScene(new Scenes::Scene());
        sm.SetActiveImmediate(newIdx);
        idx = newIdx;
    }

    bool ok = sm.LoadScene(idx, path);
    if (ok) {
        m_selectedEntityId = 0;
        _invalidateCache();
        // Immediately sync overlay world so panels reflect the loaded scene
        if (auto* overlay = Services::OverlayService::Get()) {
            auto* active = sm.GetActive();
            if (active) overlay->SetWorld(&active->GetWorld());
        }
    }
    return ok;
}
