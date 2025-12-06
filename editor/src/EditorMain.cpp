/* Start Header *****************************************************************/
/*!
\file   EditorMain.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
Main entry point for the Grape Engine Level Editor.
Launches the application in editor mode with the level editor interface.
*/
/* End Header *******************************************************************/

#include <crtdbg.h>
#include "core/Application.h"
#include "EditorApplication.h"
#include "services/Time.h"
#include "services/WindowManager.h"
#include "platform/IPlatformContext.h"

/**
 * @brief Main entry point for the Grape Engine Level Editor
 */
int main() {
    // Enable memory leak detection in debug builds
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // Initialize engine in Editor mode
    Engine::Application engine;
#ifdef _DEBUG
    engine.Initialize(Engine::EngineMode::Editor, true);
#else
    engine.Initialize(Engine::EngineMode::Editor, false);
#endif

    // Get platform context for editor use
    auto* platformContext = engine.GetPlatformContext();
    if (!platformContext) {
        return -1;
    }

    // Create editor application
    EditorApplication editor(&engine);
    editor.Initialize();

    // Initialize systems after window is created
    ECS::World emptyWorld;
    engine.GetSystemManager().CreateAll(emptyWorld);

    // Editor main loop
    while (engine.IsRunning()) {
        editor.Update();
        engine.Update();
        editor.Render();

        // Swap buffers using platform abstraction
        for (auto* win : platformContext->GetAllWindows()) {
            win->SwapBuffers();
        }
    }

    // Shutdown
    editor.Shutdown();
    engine.Shutdown();

    return 0;
}

#ifdef _WIN32
#include <windows.h>
// WinMain shim for GUI/Windows subsystem: forward to main()
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return main();
}
#endif
