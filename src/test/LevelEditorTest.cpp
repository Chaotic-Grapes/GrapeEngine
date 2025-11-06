/* Start Header *****************************************************************/
/*!
\file   LevelEditorTest.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   6th November 2025
\brief
Implements the LevelEditorTest scene which provides a test environment for the
level editor. Follows the same pattern as ECSTestScene and GraphicsTestScene
for consistent Scene-based architecture.

The scene:
- Creates a window using CREATE_WINDOW macro
- Sets up ImGui for the editor UI
- Initializes LevelEditor with the scene's ECS world
- Manages the editor update/render loop
- Handles cleanup on scene unload
*/
/* End Header *******************************************************************/

#include "LevelEditorTest.h"
#include "core/Application.h"
#include "core/Logger.h"
#include "services/WindowManager.h"
#include "services/Input.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "services/OverlayService.h"

using namespace Sandbox;

void LevelEditorTest::OnLoad() {
    // Get window configuration from application config
    const auto& config = Engine::CORE->GetConfig();
    const int windowWidth = config.WindowConfig.Width;
    const int windowHeight = config.WindowConfig.Height;

    // Create window: follows same pattern as test scenes
    CREATE_WINDOW("Level Editor", windowWidth, windowHeight);

    // Tell OverlayService to enable LevelEditor for this scene
    Services::OverlayService* overlay = Services::OverlayService::Get();
    if (overlay) {
        overlay->EnableLevelEditorForScene(this);
    }

    LOG_INFO("Level Editor Test scene initialized");
}

void LevelEditorTest::OnUpdate() {
    if (!m_levelEditor) return;

    Window* window = WindowManager::GetMainWindow();
    if (!window) return;

    // Update level editor logic (handles input and updates)
    m_levelEditor->Update();

    // Clear screen
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render level editor (includes dockspace and all panels)
    m_levelEditor->Render();

    // Swap buffers
    window->SwapBuffers();
}

void LevelEditorTest::OnUnload() {
    // Destroy level editor
    if (m_levelEditor) {
        m_levelEditor.reset();
    }

    // Just reset our pointer
    m_imguiContext = nullptr;

    LOG_INFO("Level Editor Test scene unloaded");
}
