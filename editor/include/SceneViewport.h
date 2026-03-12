/* Start Header *****************************************************************/
/*!
\file   SceneViewport.h
\author Samantha Leong (50%)
        Foo Rui Qin    (50%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
SceneViewport class for the editor viewport with editor camera, entity selection,
gizmo manipulation, and drag-to-move functionality.
*/
/* End Header *******************************************************************/

#ifndef SCENE_VIEWPORT_H
#define SCENE_VIEWPORT_H

#include "BaseViewport.h"
class TilePalettePanel;

class SceneViewport : public BaseViewport {
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
                   ECS::World* world, Scenes::SceneManager* sceneManager) override;
    ~SceneViewport() override;
    
    void BeginFrame() override;
    void HandleInWorldInteraction() override;
    void ShowEditorWindows() override;
    void EndFrame() override;
    void SetTilePalette(TilePalettePanel* panel) { m_tilePalettePanel = panel; }
    void SetGridVisible(bool v) { m_showGrid = v; }
    void SetDefaultDockspaceId(ImGuiID dockspaceId) { m_defaultDockspaceId = dockspaceId; }

private:
    void _renderViewport();
    TilePalettePanel* m_tilePalettePanel = nullptr;
    // Overlay toggles for scene helpers.
    bool m_showGrid = false;
    bool m_showAxes = false;
    bool m_showBounds = true;
    bool m_showColliders = true;
    bool m_showLights = true;
    bool m_showSelectionOutline = true;
    // Active debug view index for render graph output.
    int m_debugViewIndex = 0;
    // Current dock layout preset (1/2/4).
    int m_layoutPreset = 1; // 1, 2, or 4 viewports
    // Maximize state and restore info for the scene dock.
    bool m_maximizeViewport = false;
    ImGuiID m_restoreDockId = 0;
    ImVec2 m_restorePos = ImVec2(0.0f, 0.0f);
    ImVec2 m_restoreSize = ImVec2(0.0f, 0.0f);
    bool m_restoreDockValid = false;
    bool m_requestRestore = false;
    ImGuiID m_defaultDockspaceId = 0;
    // Gizmo snapping configuration.
    bool m_snapEnabled = false;
    float m_snapTranslate = 1.0f;
    float m_snapRotate = 15.0f;
    float m_snapScale = 0.1f;
};

#endif // SCENE_VIEWPORT_H
