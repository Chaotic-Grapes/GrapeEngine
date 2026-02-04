/* Start Header *****************************************************************/
/*!
\file   GameViewport.h
\author Samantha Leong (50%)
        Foo Rui Qin    (50%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
GameViewport class for the game view with ECS scene camera and aspect ratio
selection. Separate from the editor camera and scene editing controls.
*/
/* End Header *******************************************************************/

#ifndef GAME_VIEWPORT_H
#define GAME_VIEWPORT_H

#include "BaseViewport.h"
#include "graphics/Camera.h"

class GameViewport : public BaseViewport {
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
        ECS::World* world, Scenes::SceneManager* sceneManager) override;
    ~GameViewport() override;
    void BeginFrame() override {}
    void PrepareFrame();
    void HandleInWorldInteraction() override;
    void ShowEditorWindows() override;
    void EndFrame() override {}

private:
    void _renderViewport();
    bool _syncGameCamera(ECS::Entity entity, const ECS::Components::Camera3D& camera, float targetAspect);

    // Game window aspect ratio settings
    int m_selectedAspectRatio = 0; // Index into aspect ratio list
    bool m_freeAspect = true;      // Whether to use free aspect or fixed ratio
    Engine::Camera m_gameCamera;
};

#endif // GAME_VIEWPORT_H
