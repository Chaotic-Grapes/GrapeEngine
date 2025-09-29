#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "DebugUI.h"
#include "Input.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include "System.h"
#include <algorithm>

// Initialize static variables
bool DebugUI::m_enabled = true;
std::vector<GameObject> DebugUI::m_gameObjects;
int DebugUI::m_nextGameObjectId = 1;  // Start with 1

namespace {
    Systems::Audio* gAudioPtr = nullptr;
}

void DebugUI::AttachAudio(Systems::Audio* audio) { gAudioPtr = audio; }
void DebugUI::DetachAudio() { gAudioPtr = nullptr; }

void DebugUI::Initialize(GLFWwindow* window) {
    IMGUI_CHECKVERSION();     // Verify ImGUI version compatibility
    ImGui::CreateContext();   // Create ImGUI rendering context
    ImGuiIO& io = ImGui::GetIO();  // Get input/output config
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Allow keyboard navigation
    io.FontGlobalScale = 1.35f;    // Scale the entire UI

    ImGui::StyleColorsDark(); // Set dark theme colors
    ImGui_ImplGlfw_InitForOpenGL(window, true); // GLFW backend (window/input handling)
    ImGui_ImplOpenGL3_Init("#version 330");     // OpenGL3 backend (GPU rendering)

    // Initialize with some default game objects for testing
    AddGameObject("Player");
    AddGameObject("Enemy");
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
      _showAudioWindow(*gAudioPtr);
   

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
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
    if (ImGui::Button("Print GPU Specs to Console")) {
        Input::PrintSpecs();
    }
    ImGui::End();
}

void DebugUI::_showAudioWindow(Systems::Audio& audio) {
    // ---------- Top-level Audio Monitor ----------
    ImGui::SetNextWindowPos(ImVec2(10, 300), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360, 220), ImGuiCond_Once);
    ImGui::Begin("Audio Monitor");

    // Master volume
    {
        float mv = audio.GetMasterVolume();
        if (ImGui::SliderFloat("Master Volume", &mv, 0.0f, 1.0f, "%.2f")) {
            audio.SetMasterVolume(mv);
        }
    }

    // Toggle library window
    static bool showLibrary = false;
    if (ImGui::Button("Open Audio Library")) showLibrary = true;

    ImGui::End();

    if (!showLibrary) return;

    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Once);
    if (ImGui::Begin("Audio Library", &showLibrary)) {
        // Editor -> local library rows
        struct Row { Resources::SoundCue::Ptr cue; SoundInstance::StrongPtr inst; float vol{ 1.f }; bool loop{ false }; };
        static std::vector<Row> rows;

        // Add-by-path (no file dialog dependency)
        static char pathBuf[512] = {};
        ImGui::InputTextWithHint("##AddPath", "Paste audio path (wav/mp3/ogg/flac...)", pathBuf, sizeof(pathBuf));
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            std::string path = pathBuf;
            if (!path.empty()) {
                auto cue = Resources::SoundCue::CreateFromFile(path);
                if (cue) {
                    auto s = cue->getSettings();
                    s.Volume = 1.0f; s.Loop = false;
                    cue->setSettings(s);
                    audio.Add(cue);
                    rows.push_back(Row{ cue, nullptr, s.Volume, s.Loop });
                }
                pathBuf[0] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear List")) rows.clear();

        ImGui::Separator();

        // Optional search filter
        static char filter[128] = {};
        ImGui::InputTextWithHint("##filter", "Filter...", filter, sizeof(filter));
        const std::string filt = filter;

        ImGui::Separator();

        // Draw the rows
        for (int i = 0; i < (int)rows.size(); ++i) {
            auto& r = rows[i];
            if (!r.cue) continue;
            if (!filt.empty() && r.cue->getName().find(filt) == std::string::npos) continue;

            ImGui::PushID(i);

            ImGui::TextUnformatted(r.cue->getName().c_str());
            ImGui::SameLine();

            const bool playing = (r.inst && r.inst->IsPlaying());

            // Play / Stop / Pause / Resume
            if (!playing) {
                if (ImGui::SmallButton("Play")) {
                    auto s = r.cue->getSettings();
                    s.Volume = r.vol; s.Loop = r.loop;
                    r.cue->setSettings(s);
                    r.inst = audio.Play(r.cue);
                    if (r.inst) r.inst->InterpolateVolume(r.vol, 0.0f);
                }
            }
            else {
                if (ImGui::SmallButton("Stop")) { r.inst->Stop(); r.inst.reset(); }
            }
            ImGui::SameLine();
            if (playing) {
                if (ImGui::SmallButton("Pause")) { r.inst->Pause(); }
                ImGui::SameLine();
                if (ImGui::SmallButton("Resume")) { r.inst->Resume(); }
                ImGui::SameLine();
                ImGui::TextDisabled("[playing]");
            }

            // Loop toggle (apply immediately if playing)
            bool loop = r.loop;
            ImGui::SameLine();
            if (ImGui::Checkbox("Loop", &loop)) {
                r.loop = loop;
                if (r.inst && r.inst->IsPlaying()) {
                    // requires SoundInstance::SetLoop(bool)
                    r.inst->SetLoop(loop);
                }
                else {
                    auto s = r.cue->getSettings();
                    s.Loop = loop;
                    r.cue->setSettings(s);
                }
            }

            // Per-track volume (live adjust if playing)
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.f);
            float v = r.vol;
            if (ImGui::SliderFloat("Vol", &v, 0.0f, 1.0f, "%.2f")) {
                r.vol = v;
                if (r.inst && r.inst->IsPlaying()) {
                    r.inst->InterpolateVolume(v, 0.05f);
                }
            }

            ImGui::PopID();
        }
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
        m_gameObjects.clear();
    }

    ImGui::End();
}

// Search for an object and return pointer (or nullptr)
GameObject* DebugUI::FindGameObject(int id) {
    // Search from start to end of vector; check if each object's ID matches what we're looking for
    std::vector<GameObject>::iterator it = std::find_if(m_gameObjects.begin(), m_gameObjects.end(),
        [id](const GameObject& gameObject) { return gameObject.Id == id; });

    // If iterator reached the end, object wasn't found
    // &(*it) cause we want to dereference pointer to get object then take its address to return pointer
    return (it != m_gameObjects.end()) ? &(*it) : nullptr;
}

// Create a new object
void DebugUI::AddGameObject(const std::string& name) { 
    // Increment ID
    m_gameObjects.emplace_back(m_nextGameObjectId++, name); 
}

// Find and delete an object by ID
void DebugUI::RemoveGameObject(int id) {
    // Same thing
    std::vector<GameObject>::iterator it = std::find_if(m_gameObjects.begin(), m_gameObjects.end(),
        [id](const GameObject& gameObject) { return gameObject.Id == id; });

    // If object is found, erase
    // No need for &(*it) cause not returning anything
    if (it != m_gameObjects.end()) {
        m_gameObjects.erase(it);
    }
}

void DebugUI::Shutdown() {
    // Standard cleanup stuff
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
