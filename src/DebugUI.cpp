#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "DebugUI.h"
#include "Input.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include "ecs/World.h"

// Initialize static variables
bool DebugUI::m_enabled = true;
std::vector<GameObject> DebugUI::m_gameObjects;
World* DebugUI::m_world = nullptr;

void DebugUI::Initialize(GLFWwindow* window) {
    IMGUI_CHECKVERSION();     // Verify ImGUI version compatibility
    ImGui::CreateContext();   // Create ImGUI rendering context
    ImGuiIO& io = ImGui::GetIO();  // Get input/output config
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Allow keyboard navigation
    io.FontGlobalScale = 1.35f;    // Scale the entire UI

    ImGui::StyleColorsDark(); // Set dark theme colors
    ImGui_ImplGlfw_InitForOpenGL(window, true); // GLFW backend (window/input handling)
    ImGui_ImplOpenGL3_Init("#version 330");     // OpenGL3 backend (GPU rendering)
}

void DebugUI::NewFrame() {
    // Interact with debug UI (start of every frame so it keeps getting checked)
    if (Input::WasKeyJustPressed(GLFW_KEY_F1)) {
        DebugUI::SetEnabled(!DebugUI::IsEnabled());
    }

    if (!m_enabled) return;  // Early exit if UI is toggled off

    ImGui_ImplOpenGL3_NewFrame(); // Prepare OpenGL rendering
    ImGui_ImplGlfw_NewFrame();    // Process window/input events
    ImGui::NewFrame();            // Start ImGUI's internal frame
}

void DebugUI::Render() {
    if (!m_enabled) return;  // Early exit if UI is toggled off

    // Control variable for the ImGUI demo window
    static bool showDemo = false;
    if (showDemo) {
        ImGui::ShowDemoWindow(&showDemo);  // Show ImGUI's built-in demo window
    }

    // Render custom debug windows
    _showEngineDebugWindow(showDemo); // Pass demo control to debug window
    _showPerformanceWindow();  // Show FPS and performance stats
    _showInputDebugWindow();   // Input debugging
    _showGameObjectEditor();   // Game object editor

    // Finalize the frame and send to GPU
    ImGui::Render();  // Generate draw commands from UI
    ImDrawData* drawData = ImGui::GetDrawData();  // Get rendering data structure
    ImGui_ImplOpenGL3_RenderDrawData(drawData);   // Submit to OpenGL for GPU execution
}

void DebugUI::_showEngineDebugWindow(bool& showDemo) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);     // Position window (only on first appearance)
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Once);  // Size

    // Display current engine state info
    ImGui::Begin("GrapeEngine Debug Console");
    ImGui::Text("Engine Status: Running");
    ImGui::Text("Debug UI: Active");
    ImGui::Separator();  // Visual divider line

    // Interactive button to toggle visibility
    if (ImGui::Button("Toggle Demo Window")) {
        showDemo = !showDemo;
    }

    ImGui::Text("Press F1 to toggle debug UI");

    ImGui::End();  // Complete window definition
}

void DebugUI::_showPerformanceWindow() {
    ImGui::SetNextWindowPos(ImVec2(10, 170), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_Once);

    ImGui::Begin("Performance Monitor");
    // ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    // ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
    if (ImGui::Button("Print GPU Specs to Console")) {
        Input::PrintSpecs();
    }
    ImGui::End();
}

void DebugUI::_showInputDebugWindow() {
    // Same stuff as before
    ImGui::SetNextWindowPos(ImVec2(330, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320, 400), ImGuiCond_Once);
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
    static int spacePressed = 0;
    static int spaceReleased = 0;

    if (Input::WasKeyJustPressed(GLFW_KEY_SPACE)) spacePressed++;
    if (Input::WasKeyJustReleased(GLFW_KEY_SPACE)) spaceReleased++;

    ImGui::Text("Space Bar Events:");
    ImGui::Text("  Pressed: %d times", spacePressed);
    ImGui::Text("  Released: %d times", spaceReleased);

    if (ImGui::Button("Reset Counters")) {
        spacePressed = 0;
        spaceReleased = 0;
    }

    ImGui::End();
}

void DebugUI::_showGameObjectEditor() {
    // Position the window to the right of existing windows
    ImGui::SetNextWindowPos(ImVec2(670, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(375, 400), ImGuiCond_Once);

    ImGui::Begin("Game Object Editor");
    ImGui::Text("Level Editor Tools");
    ImGui::Separator();

    // Add new game object section
    ImGui::Text("Create New Object:");
    static char newObjectName[64] = "NewObject";  // Store new object name
    ImGui::InputText("Name", newObjectName, sizeof(newObjectName));

    // When button is clicked, create the object
    if (ImGui::Button("Add Object")) {
        AddGameObject(std::string(newObjectName));
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
    for (const GameObject& gameObject : m_gameObjects) {
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
        // Again create unique button IDs
        if (ImGui::SmallButton((std::string(gameObject.IsActive ? "Hide##" : "Show##") + 
            std::to_string(gameObject.Id)).c_str())) {
            // Find the object and toggle its state
            GameObject* obj = FindGameObject(gameObject.Id);
            if (obj) {
                obj->IsActive = !obj->IsActive;
            }
        }

        // Delete button for each object
        ImGui::SameLine();
        // Unique button IDs like Delete##1, Delete##2 (everything after ## is hidden from display)
        // Otherwise ImGUI wouldn't know which button was clicked
        if (ImGui::SmallButton(("Delete##" + std::to_string(gameObject.Id)).c_str())) {
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
        // Destroy all entities in the ECS world
        if (m_world) {
            for (const GameObject& obj : m_gameObjects) {
                Entity entity(obj.Id, m_world);
                m_world->GetEntityManager().DestroyEntity(entity);
            }
        }
        // Clear the UI list
        m_gameObjects.clear();
    }

    ImGui::End();
}

// Set the world reference and initialize default editor objects
void DebugUI::SetWorld(World* world) {
    m_world = world;

    // Add default objects when world is first set
    // (prevents duplicates on multiple calls)
    static bool defaultsAdded = false;
    if (m_world && !defaultsAdded) {
        AddGameObject("Player");
        AddGameObject("Enemy");
        defaultsAdded = true;
    }
}

// Search for an object and return pointer (or nullptr)
GameObject* DebugUI::FindGameObject(EntityId id) {
    // Search from start to end of vector; check if each object's ID matches what we're looking for
    std::vector<GameObject>::iterator it = std::find_if(m_gameObjects.begin(), m_gameObjects.end(),
        [id](const GameObject& gameObject) { return gameObject.Id == id; });

    // If iterator reached the end, object wasn't found
    // &(*it) cause we want to dereference pointer to get object then take its address to return pointer
    return (it != m_gameObjects.end()) ? &(*it) : nullptr;
}

// Create a new object
void DebugUI::AddGameObject(const std::string& name) {
    // Safety check
    if (!m_world) return;
    // Create real ECS entity with components
    Entity entity = _createGameEntity(name);
    // Add to editor list for UI display
    m_gameObjects.emplace_back(entity.GetId(), name);
}

// Find and delete an object by ID
void DebugUI::RemoveGameObject(EntityId id) {
    // Safety check
    if (!m_world) return;

    // Remove from editor list FIRST to avoid iterator issues
    std::vector<GameObject>::iterator it = std::find_if(m_gameObjects.begin(), m_gameObjects.end(),
        [id](const GameObject& gameObject) { return gameObject.Id == id; });

    // If object is found, erase
    // No need for &(*it) cause not returning anything
    if (it != m_gameObjects.end()) {
        m_gameObjects.erase(it);
    }

    // THEN remove from ECS world
    Entity entity(id, m_world);
    m_world->GetEntityManager().DestroyEntity(entity);
}

// Helper function to create entities with basic components
Entity DebugUI::_createGameEntity(const std::string& name) {
    Entity entity = m_world->CreateEntity();

    // Add basic components that most game objects need
    entity.AddComponent<Component::Transform>();

    // Add different components based on object type
    if (name == "Player") {
        entity.AddComponent<Component::SpriteRenderer>("player_sprite.png");
        entity.AddComponent<Component::Rigidbody2D>();
        entity.AddComponent<Component::BoxCollider2D>();
    }
    else if (name == "Enemy") {
        entity.AddComponent<Component::SpriteRenderer>("enemy_sprite.png");
        entity.AddComponent<Component::Rigidbody2D>();
        entity.AddComponent<Component::BoxCollider2D>();
    }
    else if (name == "Collectible") {
        entity.AddComponent<Component::SpriteRenderer>("collectible_sprite.png");
        entity.AddComponent<Component::CircleCollider2D>();
        // Make it a trigger
        auto* collider = entity.GetComponent<Component::CircleCollider2D>();
        if (collider) collider->IsTrigger = true;
    }
    else {
        // Default object: just transform and sprite
        entity.AddComponent<Component::SpriteRenderer>();
    }

    return entity;
}

void DebugUI::Shutdown() {
    // Standard cleanup stuff
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
