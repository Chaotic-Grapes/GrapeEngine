/* Start Header *****************************************************************/
/*!
\file   EditorApplication.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Editor application class that manages editor-specific functionality,
UI, and tools separate from the engine core.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITORAPPLICATION_H
#define EDITORAPPLICATION_H

#include "core/Application.h"
#include "EditorConfiguration.h"
#include "EditorState.h"
#include "EditorStartupStage.h"
#include <string>

// Forward declarations
namespace Services {
    class EditorService;
}

namespace Scenes {
    class SceneManager;
}

/**
 * @brief Editor application that manages editor UI and tools
 */
class EditorApplication {
public:
    explicit EditorApplication(Engine::Application* engine);
    ~EditorApplication();

    /**
     * @brief Initialize editor systems and UI
     */
    void Initialize();

    /**
     * @brief Begin frame processing - handle input and request picking
     */
    void BeginFrame();

    /**
     * @brief Update editor UI and tools (called after engine systems)
     */
    void Update();

    /**
     * @brief Render editor UI
     */
    void Render();

    /**
     * @brief End frame processing - resolve picking and update selection
     */
    void EndFrame();

    /**
     * @brief Shutdown editor and save state
     */
    void Shutdown();

    /**
     * @brief Get editor settings
     */
    const EditorSettings& GetSettings() const { return m_editorSettings; }

    /**
     * @brief Get current editor playback state
     * @return Current editor state (Edit, Play, Paused, Step)
     */
    EditorState GetEditorState() const;

    /**
     * @brief Initialize script manager callbacks for C# interop
     * @param scriptManager Pointer to the ScriptManager
     * @param world The ECS world for callback access
     * 
     * Sets up callbacks that allow C# hot reload code to invoke native functionality.
     */
    void InitializeScriptCallbacks(ECS::ScriptManager* scriptManager, ECS::World* world);

    EditorStartupStage GetStartupStage() const { return m_startupStage; }
    bool IsProjectInitialized() const { return m_projectInitialized; }
    const std::string& GetProjectRoot() const { return m_projectRoot; }

    void RequestProjectSelection();
    void HandleProjectSelected(const std::string& projectRoot);
    void HandleSceneSelected(const std::string& scenePath);
    void HandleContinueWithoutScene();

private:
    Engine::Application* m_engine;
    Services::EditorService* m_editorService;
    EditorSettings m_editorSettings;
    bool m_initialized = false;
    bool m_projectInitialized = false;
    bool m_projectLoadInProgress = false;
    bool m_waitForScripts = false;
    bool m_sawCompileStart = false;
    int m_compileIdleFrames = 0;
    EditorStartupStage m_startupStage = EditorStartupStage::SelectProject;
    std::string m_projectRoot;
    
    // Global pointers for script callbacks
    static Engine::Application* s_editorApplication;
    static ECS::World* s_editorWorld;

    void _loadEditorSettings();
    void _saveEditorSettings();
    void _createMainWindow();
    void _initializeEditorService();
    void _applyProjectSettings();
    void _clearScenes();
    bool _loadSceneFromPath(const std::string& scenePath);
};

#endif
