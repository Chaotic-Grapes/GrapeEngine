/* Start Header *****************************************************************/
/*!
\file   LevelEditorTest.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   6th November 2025
\brief
Declares the LevelEditorTest scene which provides a test environment for the
level editor. This scene creates a window, initializes the editor systems,
and manages the editor lifecycle within the Scene architecture.

Features:
- Creates editor window following Scene pattern
- Initializes LevelEditor with Scene's ECS world
- Manages ImGui context and rendering
- Provides clean integration with Scene management system
*/
/* End Header *******************************************************************/

#pragma once

#include "scene/Scene.h"
#include <memory>

struct ImGuiContext;

namespace Sandbox {

    class LevelEditorTest : public Scenes::Scene {
    public:
        LevelEditorTest() = default;
        ~LevelEditorTest() = default;

        /*!
        \brief Initialize the level editor scene.

        Creates the editor window, sets up ImGui, and initializes the LevelEditor
        with this scene's ECS world.
        */
        void OnLoad();

        /*!
        \brief Update the level editor each frame.

        Processes editor input and updates all editor panels.
        Also handles rendering since Scene doesn't have separate OnRender.
        */
        void OnUpdate();

        /*!
        \brief Clean up level editor resources.

        Destroys the level editor and shuts down ImGui systems.
        */
        void OnUnload();

    private:
        ImGuiContext* m_imguiContext = nullptr;      ///< ImGui context for this scene
    };

} // namespace Sandbox