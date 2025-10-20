#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "services/DebugUI.h"
#include "services/Input.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "serialization/EntitySerializer.h"
#include "core/Profiler.h"
#include "helpers/MathHelper.h"
#include <filesystem>
#include "core/Logger.h"
#include "audio/FmodAudioDevice.h"
#include "audio/SoundTypes.h"

#ifdef max
#undef max  // Undefine macro to avoid conflicts with std::max
#endif

/**
 * @file DebugUI.cpp
 * @author Foo Rui Qin
 * @date 2024
 * @brief Implementation of the debug user interface system for engine development
 * 
 * This file implements the DebugUI class which provides a comprehensive ImGui-based
 * debug interface for the game engine. The implementation includes:
 * 
 * Core Features:
 * - ImGui initialization and integration with GLFW/OpenGL backends
 * - Real-time engine status monitoring and performance metrics display
 * - Interactive game object editor with entity creation, deletion, and cloning
 * - Input debugging with event tracking and state monitoring
 * - Audio system monitoring and control interface
 * - Memory usage and profiling information display
 * 
 * UI Management:
 * - Configurable window layouts and positioning
 * - Font scaling and styling customization
 * - Cached UI elements for performance optimization
 * - Toggle-able interface with F1 key support
 * 
 * ECS Integration:
 * - Entity creation with basic components (Transform, Sprite, etc.)
 * - Real-time entity management and modification
 * - Component inspection and editing capabilities
 * - World state monitoring and debugging
 * 
 * Performance Optimizations:
 * - Cached button labels to avoid string creation every frame
 * - Efficient UI state management
 * - Minimal memory allocations during rendering
 * - Optimized ImGui usage patterns
 * 
 * The debug UI provides essential tools for engine development, debugging,
 * and performance analysis, making it easier to develop and optimize games.
 */

// Standard constructor and destructor
// raw ptr: DebugUI doesn't own the world
DebugUI::DebugUI(World* world, const DebugUIConfig& config)
    : m_config(config), m_world(world) {}

DebugUI::~DebugUI() {
    // Clean up resources only if UI was initialized
    if (m_initialized) Shutdown();
}

namespace {
    Audio::FmodAudioDevice* gAudioPtr = nullptr;
}

void DebugUI::AttachAudio(Audio::FmodAudioDevice* device) { gAudioPtr = device; }
void DebugUI::DetachAudio() { gAudioPtr = nullptr; }

void DebugUI::Initialize(GLFWwindow* window) {
    // Avoid reinitializing ImGUI
    if (!window || m_initialized) return;

    IMGUI_CHECKVERSION();     // Verify ImGUI version compatibility
    ImGui::CreateContext();   // Create ImGUI rendering context

    auto& io = ImGui::GetIO();  // Get input/output config
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
    _showAudioWindow(gAudioPtr);

    // Show ImGUI's built-in demo window
    if (m_showDemo) {
        // Toggle functionality shown later in engine debug window func
        ImGui::ShowDemoWindow(&m_showDemo);
    }

    // Finalize the frame and send to GPU
    ImGui::Render();  // Generate draw commands from UI
    auto* drawData = ImGui::GetDrawData();  // Get rendering data structure

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

// Create a new object
void DebugUI::AddGameObject(const std::string& name) {
    // Safety check
    if (name.empty() || name.length() > m_config.MAX_OBJECT_NAME_LENGTH
        || !HasValidWorld()) return;

    // Create real ECS entity with components
    auto entity = _createGameEntity(name);

    entity.Transform().Position.X = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowWidth()));
    entity.Transform().Position.Y = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowHeight()));

    // Add to editor list for UI display + clear cached toggle/delete
    // button labels so new objects get proper unique labels
    _invalidateCache();
}

// Find and delete an object by ID
void DebugUI::RemoveGameObject(const EntityId id) {
    // Safety check
    if (!HasValidWorld()) return;

    const auto entity = m_world->GetEntityManager().GetEntity(id);
    m_world->GetEntityManager().DestroyEntity(entity);

    _invalidateCache();
}

void DebugUI::CloneGameObject(const Entity& entity) {
    // Safety check
    if (!HasValidWorld()) return;

    auto cloned = entity.Clone();

    // Offset position so clone doesn't overlap original
    auto& transform = cloned.Transform();
    transform.Position.X += 50.0f;
    transform.Position.Y += 50.0f;

    _invalidateCache();
}

void DebugUI::ClearAllGameObjects() {
    // Safety check
    if (!HasValidWorld()) return;

    m_world->GetEntityManager().DestroyAllEntities();

    _invalidateCache();
}

void DebugUI::_showEngineDebugWindow() {
    // Use config values
    const auto& layout = m_config.Layout;
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
    const auto& layout = m_config.Layout;
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
        if (name == "Time")// || name == "Overlay") 
            continue; // Skip these special scopes (Time has negligible usage times)

        ImGui::Text("%s:", name.c_str());
        ImGui::BulletText("Last: %.3f ms", data.LastTimeMs);
        ImGui::BulletText("Avg:  %.3f ms", data.AverageTimeMs);
        ImGui::BulletText("Max:  %.3f ms", data.MaxTimeMs);
        ImGui::BulletText("Usage: %.1f%%", data.LastTimeMs / Profiler::GetTotalScopeTimes() * 100.f);

        if (!data.FrameTimes.empty()) {
            ImGui::PlotLines(("##" + name).c_str(),
                data.FrameTimes.data(),
                static_cast<int>(data.FrameTimes.size()),
                0,
                nullptr,
                0.0f,
                data.MaxTimeMs,
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

    ImGui::Separator();

    if (ImGui::Button("Simulate Crash")) {
        int* p = nullptr;
        *p = 42; // Dereference null pointer to cause crash
    }

    ImGui::End();
}

void DebugUI::_showAudioWindow(Audio::FmodAudioDevice* device) {
    if (!device) return;

    ImGui::SetNextWindowPos(ImVec2(10, 300), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Once);
    ImGui::Begin("Audio Monitor");

    // Master volume
    float masterVol = device->GetMasterVolume();
    if (ImGui::SliderFloat("Master Volume", &masterVol, 0.0f, 1.0f)) {
        device->SetMasterVolume(masterVol);
    }

    // Toggle library window
    static bool showLibrary = false;
    if (ImGui::Button("Open Audio Library")) showLibrary = true;

    ImGui::End();

    if (!showLibrary) return;

    ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_Once);
    if (ImGui::Begin("Audio Library", &showLibrary)) {
        struct Row {
            std::string CueId;
            std::string Path;
            Audio::PlaySettings Settings{};
            Audio::PlaybackHandle Handle{};
        };
        static std::vector<Row> rows;

        // Inputs to add a new cue
        static char cueBuf[128] = {};
        static char pathBuf[512] = {};
        ImGui::InputTextWithHint("Cue ID", "Identifier to reference this sound", cueBuf, sizeof(cueBuf));
        ImGui::InputTextWithHint("Path", "Path to audio file (wav/mp3/ogg/flac...)", pathBuf, sizeof(pathBuf));
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            std::string cue = cueBuf, path = pathBuf;
            if (!cue.empty() && !path.empty()) {
                Audio::SoundParams params{};
                params.stream = false;
                params.defaultVolume = 1.0f;
                params.is3D = false;

                if (device->LoadCue(cue, path, params)) {
                    rows.push_back(Row{ cue, path, Audio::PlaySettings{}, {} });
                    cueBuf[0] = '\0';
                    pathBuf[0] = '\0';
                } else {
                    ImGui::OpenPopup("Audio Load Error");
                }
            }
        }
        if (ImGui::BeginPopupModal("Audio Load Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Failed to load audio cue.");
            if (ImGui::Button("OK"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::Separator();

        // Rows for loaded cues
        for (size_t i = 0; i < rows.size(); ++i) {
            auto& r = rows[i];
            ImGui::PushID(static_cast<int>(i));

            ImGui::Text("Cue: %s", r.CueId.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Play")) {
                if (r.Handle)
					device->Stop(r.Handle, Audio::StopMode::Immediate);
                r.Handle = device->Play(r.CueId, r.Settings);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Stop")) {
                if (r.Handle)
                    device->Stop(r.Handle, Audio::StopMode::Immediate);
                r.Handle = {};
            }

            // Per-instance controls for next Play; if you want live editing, apply to handle as well
            ImGui::SliderFloat("Volume", &r.Settings.volume, 0.0f, 1.0f);
            ImGui::SliderFloat("Pitch", &r.Settings.pitch, 0.25f, 4.0f);
            ImGui::Checkbox("Loop", &r.Settings.loop);

            // If a handle is active, allow live volume tweak
            if (r.Handle) {
                float liveVol = r.Settings.volume;
                if (ImGui::SliderFloat("Instance Volume", &liveVol, 0.0f, 1.0f)) {
                    device->SetInstanceVolume(r.Handle, liveVol);
                }
            }

            ImGui::Separator();
            ImGui::PopID();
        }
    }

    ImGui::End();
}

void DebugUI::_showInputDebugWindow() {
    // Use config values
    const auto& layout = m_config.Layout;
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
    ImGui::Text("W: %s", Input::IsKeyDown(KEY_W) ? "PRESSED" : "Released");
    ImGui::Text("A: %s", Input::IsKeyDown(KEY_A) ? "PRESSED" : "Released");
    ImGui::Text("S: %s", Input::IsKeyDown(KEY_S) ? "PRESSED" : "Released");
    ImGui::Text("D: %s", Input::IsKeyDown(KEY_D) ? "PRESSED" : "Released");
    ImGui::Text("F1: %s", Input::IsKeyDown(GLFW_KEY_F1) ? "PRESSED" : "Released");

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
    const auto& layout = m_config.Layout;
    // Position the window to the right of existing windows
    ImGui::SetNextWindowPos(ImVec2(layout.EditorX, layout.EditorY), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(layout.EditorW, layout.EditorH), ImGuiCond_Once);

    ImGui::Begin("Game Object Editor");

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

    static char prefabName[128] = "sample-enemy-prefab";
    ImGui::InputText("Prefab Name", prefabName, sizeof(prefabName));
    if (ImGui::Button("Load Prefab") && strlen(prefabName) > 0) {
        std::ifstream file("assets/samples/" + std::string(prefabName) + ".prefab");
        if (!file.is_open()) {
            LOG_ERROR("Cannot open file: " << prefabName);
        }
        else {
            try {
                auto entityJson = nlohmann::json::parse(file);
                file.close();

                // Deserialize creates the entity internally
                (void)Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);

                _invalidateCache();
            }
            catch (const std::exception& e) {
                LOG_ERROR("Failed to parse prefab file: " << e.what());
            }
        }
    }

    // Display list of current objects
    const auto entities = m_world->GetEntityManager().GetAllEntities();
    ImGui::Text("Current Objects (%zu):", entities.size());

    // For each object
    for (const auto& entId : entities) {
        auto entity = m_world->GetEntityManager().GetEntity(entId); // Ensure entity is valid

        // Active status
        //ImGui::Text("%s", entity.GetName());
        std::stringstream oss;
        oss << "[" << entity.GetId() << "] " << entity.GetName();
        if (ImGui::CollapsingHeader(oss.str().c_str(), _getCollapsedHeaderBool(entId))) {
            // Delete button for each object
            if (ImGui::SmallButton(_getDeleteLabel(entId).c_str())) {
                RemoveGameObject(entId);
                break;
            }

            // Clone button for each object
            ImGui::SameLine();
            if (ImGui::SmallButton(_getCloneLabel(entId).c_str())) {
                CloneGameObject(entity);
                break;
            }

            ImGui::SeparatorText("Transform");

            // Get pointer to transform component to ensure we're modifying the actual component
            auto* transform = entity.GetComponent<Component::Transform>();
            if (transform) {
                // BEFORE modification
                ImGui::Text("DEBUG: Current Scale: (%.2f, %.2f)", transform->Scale.X, transform->Scale.Y);

                ImGui::Text("Position");
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("X##P" + std::to_string(entId)).c_str(), &transform->Position.X);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("Y##P" + std::to_string(entId)).c_str(), &transform->Position.Y);

                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("Rotation##" + std::to_string(entId)).c_str(), &transform->Rotation);

                ImGui::Text("Scale");
                ImGui::SetNextItemWidth(100.f);
                if (ImGui::InputFloat(std::string("X##S" + std::to_string(entId)).c_str(), &transform->Scale.X)) {
                    // Print when value changes
                    std::cout << "Scale.X changed to: " << transform->Scale.X << std::endl;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                if (ImGui::InputFloat(std::string("Y##S" + std::to_string(entId)).c_str(), &transform->Scale.Y)) {
                    std::cout << "Scale.Y changed to: " << transform->Scale.Y << std::endl;
                }
            }
        }
        //  if (ImGui::Checkbox(oss.str().c_str(), &entity.IsActive)) {
        //      Component::ShapeRenderer2D* renderer = entity.GetComponent<Component::ShapeRenderer2D>();
        //      if (renderer) {
        //          // Hide by setting alpha to 0, show by setting alpha to 1
        //          renderer->FillColor.A = entity.IsActive ? 255 : 0;
        //      }
        //  }
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
    auto entity = m_world->CreateEntity(name);

    // Add basic components that most game objects need
    entity.AddComponent<Component::Transform>();

    // Visual components
    auto& shapeRenderer = entity.AddComponent<Component::ShapeRenderer2D>();
    shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
    shapeRenderer.Radius = 35.0f;

    // Set color based on type
    if (name == "Player") shapeRenderer.FillColor = Color(0.0f, 0.0f, 1.0f, 1.0f);
    else if (name == "Enemy") shapeRenderer.FillColor = Color(1.0f, 0.0f, 0.0f, 1.0f);
    else if (name == "Collectible") shapeRenderer.FillColor = Color(1.0f, 1.0f, 0.0f, 1.0f);
    else shapeRenderer.FillColor = Color(1.0f, 1.0f, 1.0f, 1.0f);

    // Add CircleCollider2D so physics test can detect and add physics
    entity.AddComponent<Component::CircleCollider2D>(35.0f);

    return entity;
}

// Clear cached button labels
void DebugUI::_invalidateCache() {
    m_cachedDeleteLabels.clear();
    m_cachedCloneLabels.clear();
}

const std::string& DebugUI::_getDeleteLabel(const EntityId id) const {
    // Same thing
    auto it = m_cachedDeleteLabels.find(id);
    if (it == m_cachedDeleteLabels.end()) {
        // Build label
        std::string label = "Delete##" + std::to_string(id);
        it = m_cachedDeleteLabels.insert({ id, label }).first;
    }
    return it->second;
}

const std::string& DebugUI::_getCloneLabel(const EntityId id) const {
    // Same thing
    auto it = m_cachedCloneLabels.find(id);
    if (it == m_cachedCloneLabels.end()) {
        // Build label
        std::string label = "Clone##" + std::to_string(id);
        it = m_cachedCloneLabels.insert({ id, label }).first;
    }
    return it->second;
}

const bool& DebugUI::_getCollapsedHeaderBool(const EntityId id) const {
    auto it = m_cachedCollapsedHeaders.find(id);
    if (it == m_cachedCollapsedHeaders.end()) {
        it = m_cachedCollapsedHeaders.insert({ id, false }).first;
    }
    return it->second;
}
