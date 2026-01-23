/* Start Header *****************************************************************/
/*!
\file   SceneViewport.h
\author Samantha Leong (50%)
        Foo Rui Qin    (50%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
SceneViewport class for the editor viewport with editor camera, entity selection,
gizmo manipulation, and drag-to-move functionality.
*/
/* End Header *******************************************************************/

#ifndef SCENE_VIEWPORT_H
#define SCENE_VIEWPORT_H

#include "BaseViewport.h"
#include "EditorGizmo.h"
#include <unordered_map>

class SceneViewport : public BaseViewport {
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
                   ECS::World* world, Scenes::SceneManager* sceneManager) override;
    ~SceneViewport() override;
    
    void BeginFrame() override;
    void HandleInWorldInteraction() override;
    void ShowEditorWindows() override;
    void EndFrame() override;

private:
    void _renderViewport();
    
    // Static callback trampoline for C-style callback (DLL-safe)
    static void _onPickResultStatic(uint32_t entityId, void* userData) {
        if (userData) {
            static_cast<SceneViewport*>(userData)->_onPickResult(entityId);
        }
    }
    
    // Instance method to handle pick result
    void _onPickResult(uint32_t pickedEntity);
    
    // Gizmo instance for this viewport
    Editor::EditorGizmo m_gizmo;
    // Track which entity is currently being dragged (0 = none)
    uint32_t m_currentlyDraggingEntityID = 0;
};

#endif // SCENE_VIEWPORT_H
