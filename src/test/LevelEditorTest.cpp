/* Start Header *****************************************************************/
/*!
\file   LevelEditorTest.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   6th November 2025
\brief
Implements the LevelEditorTest scene which provides a test environment for the
level editor. The LevelEditor itself is managed by OverlayService.
*/
/* End Header *******************************************************************/

#include "LevelEditorTest.h"
#include "core/Application.h"
#include "core/Logger.h"
#include "services/WindowManager.h"
#include "services/Input.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "services/OverlayService.h"

using namespace Sandbox;

void LevelEditorTest::OnLoad() {
    const auto& config = Engine::CORE->GetConfig();
    const int windowWidth = config.WindowConfig.Width;
    const int windowHeight = config.WindowConfig.Height;

    // how-note: Create engine window using configured resolution for the test scene
    CREATE_WINDOW("Level Editor", windowWidth, windowHeight);

    Services::OverlayService* overlay = Services::OverlayService::Get();
    if (overlay) {
        // how-note: Hook ImGui-based LevelEditor overlay to this scene for UI rendering
        overlay->EnableLevelEditorForScene(this);
    }

    LOG_INFO("Level Editor Test scene initialized");
}

void LevelEditorTest::OnUpdate() {
    // how-note: OverlayService owns the LevelEditor UI and draws it each frame
    // Scene update can remain empty; input/rendering is managed by the overlay
}

void LevelEditorTest::OnUnload() {
    Services::OverlayService* overlay = Services::OverlayService::Get();
    if (overlay) {
        // how-note: Unregister LevelEditor overlay to avoid dangling ImGui state/resources
        overlay->DisableLevelEditor();
    }

    LOG_INFO("Level Editor Test scene unloaded");
}