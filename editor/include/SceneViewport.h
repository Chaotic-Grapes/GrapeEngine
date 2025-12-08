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

class SceneViewport : public BaseViewport {
public:
    void HandleInWorldInteraction() override;
    void ShowEditorWindows() override;

private:
    void _renderViewport();
    // Async pick request id (0 = none)
    uint32_t m_pendingPickRequestId = 0;
};

#endif // SCENE_VIEWPORT_H
