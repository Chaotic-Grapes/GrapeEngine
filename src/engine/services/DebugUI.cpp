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
#include "helpers/MathUtils.h"
#include <filesystem>
#include "core/Logger.h"
#include "audio/FmodAudioDevice.h"
#include "audio/SoundTypes.h"
#include "helpers/EntityUtils.h"
#include "scene/Scene.h"

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
// raw ptr: DebugUI doesn't own the scene
DebugUI::DebugUI(Scenes::Scene* scene, const DebugUIConfig& config)
    : m_config(config), m_scene(scene) {}

DebugUI::~DebugUI() {
    // Clean up resources only if UI was initialized
    if (m_initialized) Shutdown();
}

namespace {
    Audio::FmodAudioDevice* gAudioPtr = nullptr;
}

void DebugUI::AttachAudio(Audio::FmodAudioDevice* device) { gAudioPtr = device; }
void DebugUI::DetachAudio() { gAudioPtr = nullptr; }

void DebugUI::Initialize(GLFWwindow* pWin) {
    // Avoid reinitializing ImGUI
    if (!pWin || m_initialized) return;

    IMGUI_CHECKVERSION();     // Verify ImGUI version compatibility
    ImGui::CreateContext();   // Create ImGUI rendering context

    auto& io = ImGui::GetIO();  // Get input/output config
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Allow keyboard navigation
    io.FontGlobalScale = m_config.FontScale;    // Scale the entire UI

    ImGui::StyleColorsDark(); // Set dark theme colors
    ImGui_ImplGlfw_InitForOpenGL(pWin, true); // GLFW backend (window/input handling)
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

// Check if Scene object exists
bool DebugUI::HasValidScene() const {
    return m_scene != nullptr;
}

// Create a new object
void DebugUI::AddGameObject(const std::string& name) const {
    // Safety check
    if (name.empty() ||
        name.length() > DebugUIConfig::MAX_OBJECT_NAME_LENGTH ||
        !HasValidScene())
        return;

    // Create real ECS entity with components
    const auto entity = _createGameEntity(name);

    auto& world = m_scene->GetWorld();
    auto& transform = world.Get<ECS::Components::LocalTransform>(entity);

    transform.Position.X = MathUtils::Randomize(0.0f, static_cast<float>(Input::GetWindowWidth()));
    transform.Position.Y = MathUtils::Randomize(0.0f, static_cast<float>(Input::GetWindowHeight()));

    // Add to editor list for UI display + clear cached toggle/delete
    // button labels so new objects get proper unique labels
    _invalidateCache();
}

// Find and delete an object by ID
void DebugUI::RemoveGameObject(const PackedEntityId id) const {
    // Safety check
    if (!HasValidScene()) return;

    auto& world = m_scene->GetWorld();
    const ECS::Entity ent = ECS::EntityUtils::Unpack(id);
    if (!world.IsAlive(ent))
        return;
	world.Destroy(ent);

    _invalidateCache();
}

void DebugUI::CloneGameObject(const ECS::Entity& entity) const {
    // Safety check
    if (!HasValidScene()) return;

    auto& world = m_scene->GetWorld();
    const auto cloned = world.Clone(entity, ECS::CloneOptions{
        true
    });

    // Offset position so clone doesn't overlap original
    if (world.Has<ECS::Components::LocalTransform>(cloned)) {
        auto& transform = world.Get<ECS::Components::LocalTransform>(cloned);
        transform.Position.X += 50.0f;
        transform.Position.Y += 50.0f;
	}

    _invalidateCache();
}

void DebugUI::ClearAllGameObjects() const {
    // Safety check
    if (!HasValidScene()) return;

    auto& world = m_scene->GetWorld();
    world.Clear();

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
    ImGui::Text("Scene: %s", HasValidScene() ? "Connected" : "Not Set");

    ImGui::Separator();  // Visual divider line

    // Interactive button to toggle visibility
    if (ImGui::Button("Toggle Demo Window")) {
        m_showDemo = !m_showDemo;
    }
    ImGui::Text("Press F1 to toggle debug UI");

    ImGui::End();  // Complete window definition
}

void DebugUI::_showPerformanceWindow() const {
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

                auto& world = m_scene->GetWorld();
                // Deserialize creates the entity internally
                (void)Serialization::EntitySerializer::DeserializeEntity(world, entityJson);

                _invalidateCache();
            }
            catch (const std::exception& e) {
                LOG_ERROR("Failed to parse prefab file: " << e.what());
            }
        }
    }

    // Display list of current objects
    auto& world = m_scene->GetWorld();
    // TODO: Find a way to add count
    ImGui::Text("Current Objects:");

    world.Each<>([&](const ECS::Entity entity) {
        // Explanations for different Id usage in this block:
		// EntityId is for display only, to show unique entity index
		// PackedEntityId is for unique ImGui labels (delete/clone buttons, collapsing headers)
		// EntityId is without the generation bits, so it's easier to read
		// PackedEntityId is needed to uniquely identify entities in ImGui widgets

        // Active status
        //ImGui::Text("%s", entity.GetName());
        std::stringstream oss;
        oss << "[" << entity.Index << "] ";
        if (world.Has<ECS::Components::Name>(entity)) {
            const auto& [name] = world.Get<ECS::Components::Name>(entity);
            oss << name;
        }
        else
            oss << "Entity";

        const PackedEntityId packedEntityId = ECS::EntityUtils::Pack(entity);

        if (ImGui::CollapsingHeader(oss.str().c_str(), _getCollapsedHeaderBool(packedEntityId))) {
            // Delete button for each object
            if (ImGui::SmallButton(_getDeleteLabel(packedEntityId).c_str())) {
                RemoveGameObject(packedEntityId);
                return;
            }

            // Clone button for each object
            ImGui::SameLine();
            if (ImGui::SmallButton(_getCloneLabel(packedEntityId).c_str())) {
                CloneGameObject(entity);
                return;
            }

            ImGui::SeparatorText("Transform");

			// TODO: Modify other components too
            if (world.Has<ECS::Components::LocalTransform>(entity)) {
				auto& transform = world.Get<ECS::Components::LocalTransform>(entity);

                // BEFORE modification
                ImGui::Text("DEBUG: Current Scale: (%.2f, %.2f, %.2f)", transform.Scale.X, transform.Scale.Y, transform.Scale.Z);

                // Position
                ImGui::Text("Position");
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("X##P" + std::to_string(packedEntityId)).c_str(), &transform.Position.X);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("Y##P" + std::to_string(packedEntityId)).c_str(), &transform.Position.Y);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("Z##P" + std::to_string(packedEntityId)).c_str(), &transform.Position.Z);

                // Rotation
                ImGui::Text("Rotation");
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("X##R" + std::to_string(packedEntityId)).c_str(), &transform.Rotation.X);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("Y##R" + std::to_string(packedEntityId)).c_str(), &transform.Rotation.Y);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("Z##R" + std::to_string(packedEntityId)).c_str(), &transform.Rotation.Z);

                // Scale
                ImGui::Text("Scale");
                ImGui::SetNextItemWidth(100.f);
                if (ImGui::InputFloat(std::string("X##S" + std::to_string(packedEntityId)).c_str(), &transform.Scale.X)) {
                    // Print when value changes
                    LOG_DEBUG("Scale.X changed to: " << transform.Scale.X);
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                if (ImGui::InputFloat(std::string("Y##S" + std::to_string(packedEntityId)).c_str(), &transform.Scale.Y)) {
                    LOG_DEBUG("Scale.Y changed to: " << transform.Scale.Y);
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                if (ImGui::InputFloat(std::string("Z##S" + std::to_string(packedEntityId)).c_str(), &transform.Scale.Z)) {
                    LOG_DEBUG("Scale.Z changed to: " << transform.Scale.Z);
                }
            }
        }
    });

	//  if (ImGui::Checkbox(oss.str().c_str(), &entity.IsActive)) {
	//      Component::ShapeRenderer2D* renderer = entity.GetComponent<Component::ShapeRenderer2D>();
	//      if (renderer) {
	//          // Hide by setting alpha to 0, show by setting alpha to 1
	//          renderer->FillColor.A = entity.IsActive ? 255 : 0;
	//      }
	//  }
    ImGui::Separator();

    // Clear all buttons
    if (ImGui::Button("Clear All Objects")) {
        ClearAllGameObjects();
    }

    ImGui::End();
}

// Helper function to create entities with basic components
ECS::Entity DebugUI::_createGameEntity(const std::string& name) const {
    Color color{};

    if (name == "Player") 
        color = Color(0.0f, 0.0f, 1.0f, 1.0f);
    else if (name == "Enemy") 
        color = Color(1.0f, 0.0f, 0.0f, 1.0f);
    else if (name == "Collectible") 
        color = Color(1.0f, 1.0f, 0.0f, 1.0f);
    else 
        color = Color(1.0f, 1.0f, 1.0f, 1.0f);

    // Create new entity in ECS
    auto& world = m_scene->GetWorld();

	// Copy elision should happen here
    return world.Create(
        ECS::Components::LocalTransform{},
        ECS::Components::WorldTransform{},
        ECS::Components::ShapeCircle2D{
            35.f,
            Vector2D{},
            color
        },
        ECS::Components::CircleCollider2D{ 35.f },
        ECS::Components::Name{ *name.c_str() }
    );
}

// Clear cached button labels
void DebugUI::_invalidateCache() const {
    m_cachedDeleteLabels.clear();
    m_cachedCloneLabels.clear();
}

const std::string& DebugUI::_getDeleteLabel(const PackedEntityId id) const {
    // Same thing
    auto it = m_cachedDeleteLabels.find(id);
    if (it == m_cachedDeleteLabels.end()) {
        // Build label
        std::string label = "Delete##" + std::to_string(id);
        it = m_cachedDeleteLabels.insert({ id, label }).first;
    }
    return it->second;
}

const std::string& DebugUI::_getCloneLabel(const PackedEntityId id) const {
    // Same thing
    auto it = m_cachedCloneLabels.find(id);
    if (it == m_cachedCloneLabels.end()) {
        // Build label
        std::string label = "Clone##" + std::to_string(id);
        it = m_cachedCloneLabels.insert({ id, label }).first;
    }
    return it->second;
}

const bool& DebugUI::_getCollapsedHeaderBool(const PackedEntityId id) const {
    auto it = m_cachedCollapsedHeaders.find(id);
    if (it == m_cachedCollapsedHeaders.end()) {
        it = m_cachedCollapsedHeaders.insert({ id, false }).first;
    }
    return it->second;
}
