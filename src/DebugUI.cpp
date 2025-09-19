#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "DebugUI.h"
#include "Input.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

// Control whether UI is visible or hidden
bool DebugUI::m_enabled = true;

void DebugUI::Initialize(GLFWwindow* window) {
    IMGUI_CHECKVERSION();     // Verify ImGUI version compatibility
    ImGui::CreateContext();   // Create ImGUI rendering context
    ImGuiIO& io = ImGui::GetIO();  // Get input/output config
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Allow keyboard navigation
    io.FontGlobalScale = 1.25f;    // Scale the entire UI

    ImGui::StyleColorsDark(); // Set dark theme colors
    ImGui_ImplGlfw_InitForOpenGL(window, true); // GLFW backend (window/input handling)
    ImGui_ImplOpenGL3_Init("#version 330");     // OpenGL3 backend (GPU rendering)
}

void DebugUI::NewFrame() {
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
    ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_Once);

    ImGui::Begin("Performance Monitor");
    // ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    // ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
    if (ImGui::Button("Print GPU Specs to Console")) {
        Input::PrintSpecs();
    }
    ImGui::End();
}

void DebugUI::Shutdown() {
    // Standard cleanup stuff
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
