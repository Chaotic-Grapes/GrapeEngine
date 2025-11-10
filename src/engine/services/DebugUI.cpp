/* Start Header *****************************************************************/
/*!
\file   DebugUI.cpp
\author Foo Rui Qin (70%)
        Muhammad Nur Fadzly Bin Zulkifli (30%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
Implements the DebugUI class which provides a comprehensive ImGui-based debug
interface for the game engine.

Features:
- ImGui initialization and integration with GLFW/OpenGL backends
- Real-time engine status monitoring and performance metrics display
- Interactive game object editor with entity creation, deletion, and cloning
- Input debugging with event tracking and state monitoring
- Audio system monitoring and control interface
- Configurable window layouts and font scaling
- Cached UI elements for performance optimization
- Toggle-able interface with F1 key support
*/
/* End Header *******************************************************************/

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
#include <cstring>
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
#include "services/UICommon.h"

#ifdef max
#undef max  // Undefine macro to avoid conflicts with std::max
#endif

// Standard constructor and destructor
// raw ptr: DebugUI doesn't own the scene
DebugUI::DebugUI(ECS::World* world, const DebugUIConfig& config) {
    m_world = world;
    m_config = config;
}

DebugUI::~DebugUI() {
    // Clean up resources only if UI was initialized
    if (m_initialized) Shutdown();
}

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

// Initialize ImGui context and backends
void DebugUI::Initialize(GLFWwindow* pWin) {
    // Avoid reinitializing ImGUI
    if (!pWin || m_initialized) return;

    IMGUI_CHECKVERSION();     // Verify ImGui version compatibility against compiled backends
    ImGui::CreateContext();   // Create ImGui context (holds state, fonts, style)

    auto& io = ImGui::GetIO();  // Access global IO for config flags and font scale
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard controls (Tab/Arrow navigation)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Allow windows to dock/undock and form layouts
    io.FontGlobalScale = m_config.FontScale;              // Uniform scaling factor for all fonts/widgets

    ImGui::StyleColorsDark(); // Apply built-in dark color scheme
    ImGuiStyle& style = ImGui::GetStyle();
    style.TabBarBorderSize = 0.0f; // Remove tab bar outline for a cleaner look
    ImGui_ImplGlfw_InitForOpenGL(pWin, true); // Initialize GLFW backend (input/events)
    ImGui_ImplOpenGL3_Init("#version 330");   // Initialize OpenGL3 backend (renderer)

    m_initialized = true;  // Mark that DebugUI has been initialized
}

// Begin new ImGui frame and handle F1 toggle
void DebugUI::NewFrame() {
    if (!m_initialized) return;

    // Toggle only affects which panels are drawn, not frame lifecycle
    if (Input::IsKeyPressed(GLFW_KEY_F1)) {
        SetEnabled(!IsEnabled());
    }

    // Always begin a new ImGui frame even if UI is toggled off.
    // Rendering of debug panels remains conditional in Render().
    ImGui_ImplOpenGL3_NewFrame();  // Prepare OpenGL renderer state for this frame
    ImGui_ImplGlfw_NewFrame();     // Poll GLFW inputs and update ImGui IO
    ImGui::NewFrame();             // Reset ImGui frame state and begin UI building
}

void DebugUI::Render() {
    if (!m_initialized || !m_enabled) return;  // Early exit if UI is toggled off

    // Render custom debug windows
    _showEngineDebugWindow(); // Pass demo control to debug window
    _showPerformanceWindow();  // Show FPS and performance stats
    _showInputDebugWindow();   // Input debugging
    // _showGameObjectEditor();   // Game object editor (disabled: handled by LevelEditor)
    _showAudioWindow(m_audioPtr);

    // Show ImGUI's built-in demo window
    if (m_showDemo) {
        // Toggle functionality shown later in engine debug window func
        ImGui::ShowDemoWindow(&m_showDemo);
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

// Display engine status and demo window toggle
void DebugUI::_showEngineDebugWindow() {
    // Use config values
    UICommon::ApplyLayout(UICommon::WindowId::DEBUG_ENGINE);
    ImGui::Begin("GrapeEngine Debug Console"); // Regular window; default flags

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
    UICommon::ApplyLayout(UICommon::WindowId::DEBUG_PERF);
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
    UICommon::ApplyLayout(UICommon::WindowId::DEBUG_INPUT);
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
