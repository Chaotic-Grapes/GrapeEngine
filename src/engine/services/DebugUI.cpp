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
#include "serialization/Serializer.h"
#include "core/Profiler.h"
#include "helpers/MathUtils.h"
#include <filesystem>
#include "core/Logger.h"
#include "audio/FmodAudioDevice.h"
#include "audio/SoundTypes.h"
#include "helpers/EntityUtils.h"
#include "scene/Scene.h"
#include "services/ResourceManager.h"

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
DebugUI::DebugUI(const DebugUIConfig& config) : m_config(config) {}

// input text to string helper
static bool InputTextStdString(const char* label, std::string& str, ImGuiInputTextFlags flags = 0)
{
    char buf[256];
    // Initialize buffer with current string
    if (!str.empty())
        std::snprintf(buf, sizeof(buf), "%s", str.c_str());
    else
        buf[0] = '\0';

    bool changed = ImGui::InputText(label, buf, sizeof(buf), flags);
    if (changed) str = buf; // write result back
    return changed;
}

DebugUI::~DebugUI() {
    // Clean up resources only if UI was initialized
    if (m_initialized) Shutdown();
}

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
    _showAudioWindow(m_audioPtr);

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
	m_scenePtr = nullptr;
	m_audioPtr = nullptr;

    m_initialized = false;
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

    auto& world = m_scenePtr->GetWorld();
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

    auto& world = m_scenePtr->GetWorld();
    const ECS::Entity ent = ECS::EntityUtils::Unpack(id);
    if (!world.IsAlive(ent))
        return;
	world.Destroy(ent);

    _invalidateCache();
}

void DebugUI::CloneGameObject(const ECS::Entity& entity) const {
    // Safety check
    if (!HasValidScene()) return;

    auto& world = m_scenePtr->GetWorld();
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

    auto& world = m_scenePtr->GetWorld();
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
    ImGui::Text("Scene: %s", HasValidScene() ? "Hooked" : "Unhooked");

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
#pragma warning( push )
#pragma warning( disable : 6011 ) // Suppress null pointer dereference warning
        int* p = nullptr;
        *p = 42; // Dereference null pointer to cause crash
#pragma warning( pop )            // Restore warnings
    }

    ImGui::End();
}



    // ----------------------------------------------------------------------
   // Cached Library: quick-click lists for cached assets and loaded cues
   // ----------------------------------------------------------------------
void DebugUI::_showAudioWindow(Audio::FmodAudioDevice* device)
{
    if (!ImGui::Begin("Audio Monitor / Library")) { ImGui::End(); return; }

    ImGui::Text("Device Status: %s", device ? "Attached" : "NULL (No Audio Device)");
    ImGui::Separator();

    if (!device) {
        ImGui::TextWrapped("No audio device available.\n"
            "Call DebugUI::AttachAudio(audioService.Device()) "
            "AFTER AudioService::Initialize().");
        ImGui::End();
        return;
    }

    // Master volume
    {
        float master = device->GetMasterVolume();
        if (ImGui::SliderFloat("Master Volume", &master, 0.0f, 1.0f))
            device->SetMasterVolume(master);
    }

    ImGui::Separator();

    // Manual loader (first-time by path)
    if (ImGui::CollapsingHeader("Manual Load / Play (Path + Cue)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static std::string cueName{};
        static std::string filePath{};
        static bool loop = false;

        ImGui::SetNextItemWidth(260.f);
        InputTextStdString("Cue Name", cueName);

        ImGui::SetNextItemWidth(520.f);
        InputTextStdString("Audio File Path", filePath);
        ImGui::SameLine();
        if (ImGui::Button("Tip")) ImGui::OpenPopup("path_tip");
        if (ImGui::BeginPopup("path_tip")) {
            ImGui::TextWrapped("Use a full path once:\n"
                "  C:/project/assets/audio/bgm1.ogg\n"
                "Then switch to relative.");
            ImGui::EndPopup();
        }

        ImGui::Checkbox("Loop", &loop);

        ImGui::BeginDisabled(cueName.empty() || filePath.empty());

        if (ImGui::Button("Load Cue")) {
            Audio::SoundParams p{}; p.stream = true; p.is3D = false; // music
            if (!device->LoadCue(cueName, filePath, p)) {
                LOG_ERROR("LoadCue failed. Path=" << filePath.c_str() << " Cue=" << cueName.c_str());
            }
            else {
				LOG_INFO("Loaded Cue=" << cueName.c_str() << " from Path=" << filePath.c_str());
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Play")) {
            Audio::PlaySettings s{}; s.loop = loop; s.volume = 1.0f; s.pitch = 1.0f;
            auto h = device->PlaySingle(cueName, s, Audio::PlayPolicy::SingleInstanceRestart);
            if (!h) LOG_ERROR("Play failed. Cue may not be loaded. Cue=" << cueName.c_str());
        }

        ImGui::SameLine();

        if (ImGui::Button("Stop")) {
            device->StopCue(cueName, Audio::StopMode::Immediate);
        }

        ImGui::EndDisabled();
    }

    ImGui::Separator();

    // Cached Library
    if (ImGui::CollapsingHeader("Cached Library", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // A) FMOD-loaded cues
        {
            ImGui::TextUnformatted("Loaded Cues (FMOD)");
            if (ImGui::BeginTable("loaded_cues", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Cue Id");
                ImGui::TableSetupColumn("Source Path");
                ImGui::TableSetupColumn("Actions");
                ImGui::TableHeadersRow();

                static std::vector<std::pair<std::string, std::string>> s_cues;
                s_cues.clear();
                device->GetLoadedCues(s_cues);

                for (const auto& [cueId, path] : s_cues) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(cueId.c_str());
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(path.c_str());
                    ImGui::TableSetColumnIndex(2);

                    std::string playLbl = "Play##" + cueId;
                    std::string stopLbl = "Stop##" + cueId;

                    if (ImGui::SmallButton(playLbl.c_str())) {
                        Audio::PlaySettings s{}; s.loop = false; s.volume = 1.0f; s.pitch = 1.0f;
                        device->PlaySingle(cueId, s, Audio::PlayPolicy::SingleInstanceRestart);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(stopLbl.c_str())) {
                        device->StopCue(cueId, Audio::StopMode::Immediate);
                    }
                }
                ImGui::EndTable();
            }
        }

        ImGui::Spacing();

        // B) ResourceManager cached files (bytes cached -> Load/Play by path)
        {
            ImGui::TextUnformatted("Cached Files (ResourceManager)");
            if (ImGui::BeginTable("cached_files", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Path");
                ImGui::TableSetupColumn("Cue Id");
                ImGui::TableSetupColumn("Actions");
                ImGui::TableHeadersRow();

                const auto cached = RM.ListCachedAudioPaths(); // requires your small RM helper
                static std::unordered_map<size_t, std::string> s_rowCueId;

                for (size_t i = 0; i < cached.size(); ++i) {
                    const std::string& path = cached[i];
                    ImGui::TableNextRow();

                    // path
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(path.c_str());

                    // derived default cue
                    ImGui::TableSetColumnIndex(1);
                    std::string defaultCue = path;
                    if (auto pos = defaultCue.find_last_of("/\\"); pos != std::string::npos)
                        defaultCue = defaultCue.substr(pos + 1);
                    if (auto dot = defaultCue.find_last_of('.'); dot != std::string::npos)
                        defaultCue = defaultCue.substr(0, dot);

                    auto& editCue = s_rowCueId[i];
                    if (editCue.empty()) editCue = defaultCue;

                    std::string inputId = "##cue_" + std::to_string(i);
                    ImGui::SetNextItemWidth(200.0f);
                    InputTextStdString(inputId.c_str(), editCue);

                    // actions
                    ImGui::TableSetColumnIndex(2);
                    std::string loadLbl = "Load##" + std::to_string(i);
                    std::string playLbl = "Load+Play##" + std::to_string(i);

                    if (ImGui::SmallButton(loadLbl.c_str())) {
                        Audio::SoundParams p{}; p.stream = true; p.is3D = false;
                        if (!device->LoadCue(editCue, path, p)) {
                            LOG_ERROR("LoadCue failed for cached path: " << path.c_str());
                        }
                        else {
                            LOG_INFO("Loaded cue: " << editCue.c_str() << " from cached path: " << path.c_str());
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(playLbl.c_str())) {
                        Audio::SoundParams p{}; p.stream = true; p.is3D = false;
                        if (!device->LoadCue(editCue, path, p)) {
                            LOG_ERROR("LoadCue failed for cached path: " << path.c_str());
                        }
                        else {
                            Audio::PlaySettings s{}; s.loop = false; s.volume = 1.0f; s.pitch = 1.0f;
                            device->PlaySingle(editCue, s, Audio::PlayPolicy::SingleInstanceRestart);
                        }
                    }
                }
                ImGui::EndTable();
            }
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

    // TODO: Use Serializer
    static char prefabName[128] = "sample-enemy-prefab";
    ImGui::InputText("Prefab Name", prefabName, sizeof(prefabName));
    if (ImGui::Button("Load Prefab") && strlen(prefabName) > 0) {
        std::string path = "assets/samples/" + std::string(prefabName) + ".prefab";
        auto& world = m_scenePtr->GetWorld();
        if (!Serialization::EntitySerializer::LoadPrefab(path, world)) {
            LOG_ERROR("Failed to load prefab: " << path);
        }
        else {
            _invalidateCache();
        }
    }

    // TODO: Find a way to add count
    ImGui::Text("Current Objects:");

    if (HasValidScene()) {
        auto& world = m_scenePtr->GetWorld();

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

                ImGui::SameLine();
                if (ImGui::SmallButton("Save Prefab")) {
                    // Build a name from the entity name or index
                    std::string saveName = "entity-" + std::to_string(entity.Index);
                    if (world.Has<ECS::Components::Name>(entity)) {
                        const auto& [name] = world.Get<ECS::Components::Name>(entity);
                        if (std::strlen(name) > 0)
                            saveName = name;
                    }
                    // sanitize simple characters (replace spaces)
                    for (auto& c : saveName) if (c == ' ') c = '_';

                    std::string path = "assets/samples/" + saveName + ".prefab";
                    if (!Serialization::EntitySerializer::SavePrefab(path, world, entity)) {
                        LOG_ERROR("Failed to save prefab: " << path);
                    }
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
    }

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
    auto& world = m_scenePtr->GetWorld();

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
