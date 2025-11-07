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
#include "services/UICommon.h"
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

    // Begin a new ImGui frame: backend new-frame calls, then core NewFrame
    ImGui_ImplOpenGL3_NewFrame();  // Prepare OpenGL renderer state for this frame
    ImGui_ImplGlfw_NewFrame();     // Poll GLFW inputs and update ImGui IO
    ImGui::NewFrame();             // Reset ImGui frame state and begin UI building
}

// Render all debug windows
void DebugUI::Render() {
    if (!m_initialized) return;

    // Draw debug panels only when enabled
    if (m_enabled) {
        _showEngineDebugWindow();
        _showPerformanceWindow();
        _showInputDebugWindow();
        _showAudioWindow(m_audioPtr);


        if (m_showDemo) {
            ImGui::ShowDemoWindow(&m_showDemo);
        }
    }
}

// Shutdown ImGui and cleanup resources
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

    ImGui::Begin("Performance Monitor"); // Regular window; default flags

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

// Display audio system controls and library window
void DebugUI::_showAudioWindow(Audio::FmodAudioDevice* device) {
    if (!device) return;

    UICommon::ApplyLayout(UICommon::WindowId::DEBUG_AUDIO);
    ImGui::Begin("Audio Monitor");

    // Master volume
    float masterVol = device->GetMasterVolume();
    if (ImGui::SliderFloat("Master Volume", &masterVol, 0.0f, 1.0f)) {
        device->SetMasterVolume(masterVol);
    }

    ImGui::Separator();

    // Toggle library window
    static bool showLibrary = false;
    if (ImGui::Button("Open Audio Library")) showLibrary = true;

    ImGui::End();

    if (!showLibrary) return;

    ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_Once); // One-time suggested size for the library window
    if (ImGui::Begin("Audio Library", &showLibrary)) { // Closeable window controlled by 'showLibrary'
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
                }
                else {
                    ImGui::OpenPopup("Audio Load Error");
                }
            }
        }
        // Modal popup auto-sizes to fit its content every frame
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

// Display mouse, keyboard and window input state
void DebugUI::_showInputDebugWindow() {
    // Use config values
    UICommon::ApplyLayout(UICommon::WindowId::DEBUG_INPUT);

    ImGui::Begin("Input Debug"); // Regular window; default flags

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
