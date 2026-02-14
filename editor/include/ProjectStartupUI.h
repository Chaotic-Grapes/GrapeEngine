/* Start Header *****************************************************************/
/*!
\file   ProjectStartupUI.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
This header defines the ProjectStartupUI class which manages the project and
scene selection UI during the editor's startup process.

The ProjectStartupUI class renders different UI screens based on the current
EditorStartupStage, allowing the user to select a project, view a booting screen,
and pick a scene to open. It communicates user selections back to the editor
through callback functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef PROJECT_STARTUP_UI_H
#define PROJECT_STARTUP_UI_H

#include "EditorStartupStage.h"
#include "EditorConfiguration.h"
#include <functional>
#include <string>

namespace Editor {

    /**
     * @brief A struct to hold callback functions for project and scene selection events during startup.
     * The editor will call these callbacks when the user selects a project or scene, allowing the editor to 
     * respond to these events and proceed with loading the selected project/scene.
     * - OnProjectSelected: Called when the user selects a project, passing the selected project's path.
     * - OnSceneSelected: Called when the user selects a scene within the project, passing the selected scene's path.
     * - OnContinueWithoutScene: Called when the user chooses to continue without selecting a scene, 
     * allowing the editor to proceed with an empty scene or default state.
     */
    struct ProjectStartupCallbacks {
        std::function<void(const std::string&)> OnProjectSelected;
        std::function<void(const std::string&)> OnSceneSelected;
        std::function<void()> OnContinueWithoutScene;
    };

    /**
     * @brief The ProjectStartupUI class manages the user interface for project and scene selection during the editor's 
     * startup process.
     * It renders different screens based on the current EditorStartupStage, allowing the user to select a project, view 
     * a booting screen, and pick a scene to open. The class uses callback functions to communicate user selections back to the editor.
     */
    class ProjectStartupUI {
    public:
    
        /**
         * @brief Initializes the ProjectStartupUI with the given editor settings and callback functions.
         * @param settings A pointer to the EditorSettings struct containing configuration settings for the editor.
         */
        void SetEditorSettings(EditorSettings* settings) { m_settings = settings; }
        
        /**
         * @brief Sets the callback functions for project and scene selection events.
         * @param callbacks A ProjectStartupCallbacks struct containing the callback functions to be called on user selections.
         */
        void SetCallbacks(ProjectStartupCallbacks callbacks) { m_callbacks = std::move(callbacks); }

        /**
         * @brief Sets a getter function to query the current EditorStartupStage, allowing the UI to render the appropriate screen based on the startup stage.
         * @param getter A std::function that returns the current EditorStartupStage when called.
         */
        void SetStageGetter(std::function<EditorStartupStage()> getter) { m_stageGetter = std::move(getter); }

        /**
         * @brief Gets the current stage getter function.
         * @return A const reference to the std::function that returns the current EditorStartupStage
         */
        const std::function<EditorStartupStage()>& GetStageGetter() const { return m_stageGetter; }

        /**
         * @brief Requests to show the project browser UI, which allows the user to select a project to open.
         * This can be called from other parts of the editor to force the project selection screen to appear.
         */
        void RequestProjectBrowser() { m_forceProjectBrowser = true; }
        bool WantsProjectBrowser() const { return m_forceProjectBrowser; }

        /**
         * @brief Renders the ProjectStartupUI, displaying the appropriate screen based on the current EditorStartupStage.
         */
        void Render();

    private:
        // -------------------------------------------------------------------------
        // Internal Helpers
        // -------------------------------------------------------------------------

        // Render screens for each startup stage
        void _renderProjectBrowser();
        void _renderBootingScreen();
        void _renderScenePicker();

        // Helper to open the selected project and notify the editor via callback
        std::string _pickFolder();
        void _createProject(const std::string& projectName, const std::string& parentFolder);
        void _createScene(const std::string& sceneName);
        void _openFolder(const std::string& path);

        EditorSettings* m_settings = nullptr;               // Pointer to editor settings for accessing recent projects and other config
        ProjectStartupCallbacks m_callbacks;                // Struct holding callback functions to notify the editor of user selections
        std::function<EditorStartupStage()> m_stageGetter;  // Getter function to query the current EditorStartupStage for rendering the appropriate UI
        bool m_forceProjectBrowser = false;                 // Flag to force showing the project browser, can be set by other parts of the editor to trigger project selection

        // Project creation state
        char m_projectNameBuffer[128] = "NewProject";
        char m_projectLocationBuffer[256] = "";
        char m_sceneNameBuffer[128] = "Main";
        bool m_projectLocationInitialized = false;
        bool m_openNewProjectModal = false;
        bool m_openDeleteConfirm = false;
        std::string m_cachedProjectRoot;
        std::string m_selectedProjectPath;
        std::string m_deleteTargetPath;
        bool m_projectSelectionLocked = false;
        int m_selectedSceneIndex = -1;
    };
}

#endif
