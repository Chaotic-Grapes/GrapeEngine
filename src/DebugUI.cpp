#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "DebugUI.h"
#include "Input.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <iostream>
#include "Profiler.h"
#include "ecs/World.h"
#include "Math/MathHelper.h"

// Standard constructor and destructor
// raw ptr: DebugUI doesn't own the world
DebugUI::DebugUI(World* world, const DebugUIConfig& config)
    : m_config(config), m_world(world) {}

DebugUI::~DebugUI() {
    // Clean up resources only if UI was initialized
    if (m_initialized) Shutdown();
}

void DebugUI::Initialize(GLFWwindow* window) {
    // Avoid reinitializing ImGUI
    if (!window || m_initialized) return;

    IMGUI_CHECKVERSION();     // Verify ImGUI version compatibility
    ImGui::CreateContext();   // Create ImGUI rendering context

    ImGuiIO& io = ImGui::GetIO();  // Get input/output config
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Allow keyboard navigation
    io.FontGlobalScale = m_config.FontScale;    // Scale the entire UI

    ImGui::StyleColorsDark(); // Set dark theme colors
    ImGui_ImplGlfw_InitForOpenGL(window, true); // GLFW backend (window/input handling)
    ImGui_ImplOpenGL3_Init("#version 330");     // OpenGL3 backend (GPU rendering)

    m_initialized = true;  // Mark that DebugUI has been initialized
}

void DebugUI::NewFrame() {
    // Check
    if (!m_initialized) return;

    // Interact with debug UI (start of every frame so it keeps getting checked)
    if (Input::IsKeyPressed(GLFW_KEY_F1)) {
        SetEnabled(!IsEnabled());
    }

    if (!m_enabled) return;  // Early exit if UI is toggled off

    ImGui_ImplOpenGL3_NewFrame(); // Prepare OpenGL rendering
    ImGui_ImplGlfw_NewFrame();    // Process window/input events
    ImGui::NewFrame();            // Start ImGUI's internal frame
}

void DebugUI::Render() {
    if (!m_initialized || !m_enabled) return;  // Early exit if UI is toggled off

    // Render custom debug windows
    _showEngineDebugWindow(); // Pass demo control to debug window
    _showPerformanceWindow();  // Show FPS and performance stats
    _showInputDebugWindow();   // Input debugging
    _showGameObjectEditor();   // Game object editor

    // Show ImGUI's built-in demo window
    if (m_showDemo) {
        // Toggle functionality shown later in engine debug window func
        ImGui::ShowDemoWindow(&m_showDemo);
    }

    // Finalize the frame and send to GPU
    ImGui::Render();  // Generate draw commands from UI
    ImDrawData* drawData = ImGui::GetDrawData();  // Get rendering data structure

    if (drawData) {
        // Submit to OpenGL for GPU execution
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
}

void DebugUI::Shutdown() {
    if (!m_initialized) return;

    // Standard cleanup stuff
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_initialized = false;
}

// Check if World object exists
bool DebugUI::HasValidWorld() const {
    return m_world != nullptr;
}

// Scan world for existing entities and populate GameObject list
void DebugUI::SyncWithWorld() {
    if (!HasValidWorld()) return;

    // Clear existing list
    m_gameObjects.clear();

    // Get all entity IDs from world
    std::vector<EntityId> allEntities = m_world->GetEntityManager().GetAllEntities();

    // Iterate over every Entity ID
    for (EntityId id : allEntities) {
        // Get the actual Entity object for this ID
        Entity entity = m_world->GetEntityManager().GetEntity(id);
        // Transform (position data)
        Component::Transform* transform = entity.GetComponent<Component::Transform>();

        // If Entity has an actual transform
        if (transform) {
            // Get name or use fallback name "Entity"
            std::string entityName = m_world->GetEntityManager().GetName(id);
            if (entityName.empty()) entityName = "Entity";

            // Sync position data, add synced object to debug UI list
            GameObject gameObj(id, entityName);
            gameObj.X = transform->Position.X;
            gameObj.Y = transform->Position.Y;
            m_gameObjects.push_back(gameObj);
        }
    }
    // Refresh any cached button state now that the object list has changed
    _invalidateButtonCache();
}

// Search for an object and return pointer (or nullptr)
GameObject* DebugUI::FindGameObject(EntityId id) {
    // Search for game object in list by EntityId
    for (GameObject& obj : m_gameObjects) {
        // If found, return ptr to that object
        if (obj.Id == id) return &obj;
    }
    // Not found
    return nullptr;
}

// Create a new object
void DebugUI::AddGameObject(const std::string& name) {
    // Safety check
    if (name.empty() || name.length() > m_config.MAX_OBJECT_NAME_LENGTH
        || !HasValidWorld()) return;

    // Create real ECS entity with components
    Entity entity = _createGameEntity(name);

    GameObject gameObj(entity.GetId(), name);
    gameObj.X = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowWidth()));
    gameObj.Y = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowHeight()));

    // Sync with ECS Transform
    Component::Transform& transform = entity.Transform();
    transform.Position.X = gameObj.X;
    transform.Position.Y = gameObj.Y;

    // Add to editor list for UI display + clear cached toggle/delete
    // button labels so new objects get proper unique labels
    m_gameObjects.push_back(gameObj);
    _invalidateButtonCache();
}

// Find and delete an object by ID
void DebugUI::RemoveGameObject(EntityId id) {
    // Safety check
    if (!HasValidWorld()) return;

    // Loop through game objects
    for (std::vector<GameObject>::iterator it = m_gameObjects.begin();
        it != m_gameObjects.end(); it++) {
        // If this object matches the ID
        if (it->Id == id) {
            // Remove from ECS world first
            Entity entity(id, m_world);  // Temporary handle pointing to ECS world to use in below func
            m_world->GetEntityManager().DestroyEntity(entity); // Wants an Entity handle (not ID)

            // Remove from UI list + clear cached toggle/delete button labels
            m_gameObjects.erase(it);
            _invalidateButtonCache();

            // Already found the object so no need to keep looping
            break;
        }
    }
}

void DebugUI::ClearAllGameObjects() {
    // Safety check
    if (!HasValidWorld()) return;

    // Loop through game objects, same thing as above
    for (const GameObject& obj : m_gameObjects) {
        Entity entity(obj.Id, m_world);
        m_world->GetEntityManager().DestroyEntity(entity);
    }

    // Clear vector, clear caches 
    m_gameObjects.clear();
    _invalidateButtonCache();
}

void DebugUI::_showEngineDebugWindow() {
    // Use config values
    const DebugUIConfig::WindowLayout& layout = m_config.Layout;
    ImGui::SetNextWindowPos(ImVec2(layout.EngineX, layout.EngineY), ImGuiCond_Once);   // Position window (only on first appearance)
    ImGui::SetNextWindowSize(ImVec2(layout.EngineW, layout.EngineH), ImGuiCond_Once);  // Size

    // Display current engine state info
    ImGui::Begin("GrapeEngine Debug Console");
    ImGui::Text("Engine Status: Running");
    ImGui::Text("Debug UI: %s", m_enabled ? "Active" : "Inactive");
    ImGui::Text("World: %s", HasValidWorld() ? "Connected" : "Not Set");

    ImGui::Separator();  // Visual divider line

    // Interactive button to toggle visibility
    if (ImGui::Button("Toggle Demo Window")) {
        m_showDemo = !m_showDemo;
    }
    ImGui::Text("Press F1 to toggle debug UI");

    ImGui::End();  // Complete window definition
}

void DebugUI::_showPerformanceWindow() {
    // Use config values
    const DebugUIConfig::WindowLayout& layout = m_config.Layout;
    ImGui::SetNextWindowPos(ImVec2(layout.PerfX, layout.PerfY), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(layout.PerfW, layout.PerfH), ImGuiCond_Once);

    ImGui::Begin("Performance Monitor");

    // FPS and frame time
    ImGui::Text("FPS: %.1f", Profiler::GetFPS());
    ImGui::Text("Frame Time: %.3f ms", Profiler::GetFrameTimeMs());

    static int fpsCap = Time::FpsCap();
    ImGui::InputInt("FPS Cap", &fpsCap);
    fpsCap = std::max(fpsCap, 0); // 0 = uncapped
    if (ImGui::Button("Apply FPS Cap")) {
        Time::FpsCap(fpsCap);
    }

    ImGui::Separator();

    // Scope data
    const auto& scopes = Profiler::GetAllScopeData();
    for (const auto& [name, data] : scopes) {
		if (name == "Time" || name == "Overlay") 
            continue; // Skip these special scopes

        ImGui::Text("%s:", name.c_str());
        ImGui::BulletText("Last: %.3f ms", data.lastTimeMs);
        ImGui::BulletText("Avg:  %.3f ms", data.avgTimeMs);
        ImGui::BulletText("Max:  %.3f ms", data.maxTimeMs);

        if (!data.frameTimes.empty()) {
            ImGui::PlotLines(("##" + name).c_str(),
                data.frameTimes.data(),
                static_cast<int>(data.frameTimes.size()),
                0,
                nullptr,
                0.0f,
                data.maxTimeMs,
                ImVec2(0, 50));
        }
    }

    ImGui::Separator();

    // Reset profiler history
    if (ImGui::Button("Clear Performance History")) {
        Profiler::ClearHistory();
    }

	// Print GPU specs button
    if (ImGui::Button("Print GPU Specs to Console")) {
        Input::PrintSpecs();
    }

    ImGui::End();
}

void DebugUI::_showInputDebugWindow() {
    // Use config values
    const DebugUIConfig::WindowLayout& layout = m_config.Layout;
    // Same stuff as before
    ImGui::SetNextWindowPos(ImVec2(layout.InputX, layout.InputY), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(layout.InputW, layout.InputH), ImGuiCond_Once);

    ImGui::Begin("Input Debug");

    ImGui::Text("=== Mouse ===");
    ImGui::Text("Position: (%.1f, %.1f)", Input::GetMouseX(), Input::GetMouseY());
    ImGui::Text("Left Button: %s", Input::IsMousePressed(MOUSE_LEFT) ? "PRESSED" : "Released");
    ImGui::Text("Right Button: %s", Input::IsMousePressed(MOUSE_RIGHT) ? "PRESSED" : "Released");
    ImGui::Text("Scroll Offset: (%.1f, %.1f)", Input::GetScrollX(), Input::GetScrollY());

    ImGui::Separator();
    ImGui::Text("=== Window ===");
    ImGui::Text("Size: %dx%d", Input::GetWindowWidth(), Input::GetWindowHeight());

    ImGui::Separator();
    ImGui::Text("=== Keyboard ===");

    // Show WASD keys using constants
    ImGui::Text("W: %s", Input::IsKeyPressed(KEY_W) ? "PRESSED" : "Released");
    ImGui::Text("A: %s", Input::IsKeyPressed(KEY_A) ? "PRESSED" : "Released");
    ImGui::Text("S: %s", Input::IsKeyPressed(KEY_S) ? "PRESSED" : "Released");
    ImGui::Text("D: %s", Input::IsKeyPressed(KEY_D) ? "PRESSED" : "Released");
    ImGui::Text("F1: %s", Input::IsKeyPressed(GLFW_KEY_F1) ? "PRESSED" : "Released");

    ImGui::Separator();
    ImGui::Text("=== Event Testing ===");

    // For fun
    if (Input::IsKeyPressed(GLFW_KEY_SPACE)) m_spacePressed++;
    if (Input::IsKeyUp(GLFW_KEY_SPACE)) m_spaceReleased++;

    ImGui::Text("Space Bar Events:");
    ImGui::Text("  Pressed: %d times", m_spacePressed);
    ImGui::Text("  Released: %d times", m_spaceReleased);

    if (ImGui::Button("Reset Counters")) {
        m_spacePressed = 0;
        m_spaceReleased = 0;
    }

    ImGui::End();
}

void DebugUI::_showGameObjectEditor() {
    // Same same
    const DebugUIConfig::WindowLayout& layout = m_config.Layout;
    // Position the window to the right of existing windows
    ImGui::SetNextWindowPos(ImVec2(layout.EditorX, layout.EditorY), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(layout.EditorW, layout.EditorH), ImGuiCond_Once);

    ImGui::Begin("Game Object Editor");
    ImGui::Text("Level Editor Tools");
    ImGui::Separator();

    // Add new game object section
    ImGui::Text("Create New Object:");
    static char nameBuffer[DebugUIConfig::MAX_OBJECT_NAME_LENGTH];
    if (m_newObjectName.length() < sizeof(nameBuffer)) {
        // Store new object name
        strcpy_s(nameBuffer, m_newObjectName.c_str());
    }

    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        m_newObjectName = nameBuffer;
    }

    // When button is clicked, create the object
    if (ImGui::Button("Add Object") && !m_newObjectName.empty()) {
        AddGameObject(m_newObjectName);
        m_newObjectName = "NewObject"; // Reset
    }

    ImGui::Separator();

    // Quick add buttons for common objects
    ImGui::Text("Quick Add:");
    if (ImGui::Button("Player")) AddGameObject("Player");
    ImGui::SameLine();  // Make the next button appear on the same line instead of below
    if (ImGui::Button("Enemy")) AddGameObject("Enemy");
    ImGui::SameLine();
    if (ImGui::Button("Collectible")) AddGameObject("Collectible");

    ImGui::Separator();

    // Display list of current objects
    ImGui::Text("Current Objects (%zu):", m_gameObjects.size());

    // For each object
    for (GameObject& gameObject : m_gameObjects) {
        // Show object info
        ImGui::Text("ID: %d - %s", gameObject.Id, gameObject.Name.c_str());

        // Status indicator
        ImGui::SameLine();
        if (gameObject.IsActive) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "(Active)");
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "(Inactive)");
        }

        // Toggle active/inactive button
        ImGui::SameLine();
        // If object is active, button shows deactivate; if object is inactive, other way around
        if (ImGui::SmallButton(_getToggleLabel(gameObject).c_str())) {
            gameObject.IsActive = !gameObject.IsActive;

            // Apply to actual ECS entity
            Entity entity(gameObject.Id, m_world);
            Component::ShapeRenderer2D* renderer = entity.GetComponent<Component::ShapeRenderer2D>();
            if (renderer) {
                // Hide by setting alpha to 0, show by setting alpha to 1
                renderer->FillColor.A = gameObject.IsActive ? 255 : 0;
            }
        }

        // Delete button for each object
        ImGui::SameLine();
        // Unique button IDs
        if (ImGui::SmallButton(_getDeleteLabel(gameObject).c_str())) {
            RemoveGameObject(gameObject.Id);
            break;
        }

        // What we see for each object:
        /* ID: 1 - Player (Active) [Deactivate] [Delete]
           ID: 2 - Enemy (Inactive) [Activate] [Delete] */
    }
    ImGui::Separator();

    // Clear all buttons
    if (ImGui::Button("Clear All Objects")) {
        ClearAllGameObjects();
    }

    ImGui::End();
}

// Helper function to create entities with basic components
Entity DebugUI::_createGameEntity(const std::string& name) {
    // Create new entity in ECS
    Entity entity = m_world->CreateEntity(name);

    // Add basic components that most game objects need
    entity.AddComponent<Component::Transform>();

    // Set position based on GameObject data
    auto& shapeRenderer = entity.AddComponent<Component::ShapeRenderer2D>();
    shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
    shapeRenderer.Radius = 25.0f;

    // Set color based on type
    if (name == "Player") {
        shapeRenderer.FillColor = Color(0.0f, 0.0f, 1.0f, 1.0f);
    }
    else if (name == "Enemy") {
        shapeRenderer.FillColor = Color(1.0f, 0.0f, 0.0f, 1.0f);
    }
    else if (name == "Collectible") {
        shapeRenderer.FillColor = Color(1.0f, 1.0f, 0.0f, 1.0f); 
    }
    else {
        shapeRenderer.FillColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Add CircleCollider2D so physics test can detect and add physics
    entity.AddComponent<Component::CircleCollider2D>(25.0f);

    return entity;
}

// Clear cached button labels
void DebugUI::_invalidateButtonCache() {
    m_cachedToggleLabels.clear();
    m_cachedDeleteLabels.clear();
}

const std::string& DebugUI::_getToggleLabel(const GameObject& obj) const {
    std::unordered_map<EntityId, std::string>::iterator it = m_cachedToggleLabels.find(obj.Id);
    // If label for obj.Id doesn't exist
    if (it == m_cachedToggleLabels.end()) {
        // Build label (Hide##1233 if active, Show##123 if inactive)
        std::string label = (obj.IsActive ? "Hide##" : "Show##") + std::to_string(obj.Id);
        // Insert new label into cache
        it = m_cachedToggleLabels.insert({ obj.Id, label }).first;
    }
    // Return ref to cached string
    return it->second;
}

const std::string& DebugUI::_getDeleteLabel(const GameObject& obj) const {
    // Same thing
    std::unordered_map<EntityId, std::string>::iterator it = m_cachedDeleteLabels.find(obj.Id);
    if (it == m_cachedDeleteLabels.end()) {
        // Build label
        std::string label = "Delete##" + std::to_string(obj.Id);
        it = m_cachedDeleteLabels.insert({ obj.Id, label }).first;
    }
    return it->second;
}
