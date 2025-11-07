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

    CREATE_WINDOW("Level Editor", windowWidth, windowHeight);

    Services::OverlayService* overlay = Services::OverlayService::Get();
    if (overlay) {
        overlay->EnableLevelEditorForScene(this);
    }

    LOG_INFO("Level Editor Test scene initialized");
}

void LevelEditorTest::OnUpdate() {
    // OverlayService handles all rendering
}

void LevelEditorTest::OnUnload() {
    Services::OverlayService* overlay = Services::OverlayService::Get();
    if (overlay) {
        overlay->DisableLevelEditor();
    }

    LOG_INFO("Level Editor Test scene unloaded");
}