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
    /**
     * @brief Initialize the game viewport with fonts, ECS world, and scene manager.
     * @param mainFont Primary UI font.
     * @param boldFont Bold UI font.
     * @param symbolsFont Icon/symbol font.
     * @param world Active ECS world.
     * @param sceneManager Scene manager used for scene operations.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
        ECS::World* world, Scenes::SceneManager* sceneManager) override;

    ~GameViewport() override;

    /** @brief Per-frame hook called before game viewport work (no-op). */
    void BeginFrame() override {}

    /** @brief Sync the game camera and prepare the render target for this frame. */
    void PrepareFrame();

    /** @brief Handle in-world input interactions for the game view. */
    void HandleInWorldInteraction() override;

    /** @brief Render game viewport UI windows (aspect ratio selector, immersive toggle). */
    void ShowEditorWindows() override;

    /** @brief Per-frame hook called after game viewport work (no-op). */
    void EndFrame() override {}

    /**
     * @brief Check whether immersive (game-only fullscreen) mode is active.
     * @return True while the game viewport is displayed fullscreen.
     */
    bool IsImmersiveModeEnabled() const { return m_immersiveMode; }

private:
    /** @brief Render the game scene texture into the ImGui viewport window. */
    void _renderViewport();
    /**
     * @brief Sync the editor game camera to an ECS Camera3D component.
     * @param entity Entity that owns the Camera3D component.
     * @param camera Camera3D component data to apply.
     * @param targetAspect Desired output aspect ratio.
     * @return True if the camera was successfully synced.
     */
    bool _syncGameCamera(ECS::Entity entity, const ECS::Components::Camera3D& camera, float targetAspect);

    // Game window aspect ratio settings
    int m_selectedAspectRatio = 0; // Index into aspect ratio list
    bool m_freeAspect = true;      // Whether to use free aspect or fixed ratio
    bool m_immersiveMode = false;  // True while rendering game-only fullscreen view
    ImGuiID m_restoreDockId = 0;
    ImVec2 m_restorePos = ImVec2(0.0f, 0.0f);
    ImVec2 m_restoreSize = ImVec2(0.0f, 0.0f);
    bool m_restoreDockValid = false;
    bool m_requestRestore = false;
    Engine::Camera m_gameCamera;
};

#endif // GAME_VIEWPORT_H
