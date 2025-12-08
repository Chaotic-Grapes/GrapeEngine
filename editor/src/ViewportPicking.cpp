/* Start Header *****************************************************************/
/*!
\file   ViewportPicking.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\brief
Implementation of editor-side GPU picking utility.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ViewportPicking.h"
#include "ecs/systems/RendererSystem.h"
#include "graphics/framebuffer.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Editor {

    uint32_t ViewportPicking::RequestAsyncPick(
        float screenX,
        float screenY,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        ECS::RendererSystem* rendererSystem)
    {
        if (!rendererSystem) return 0;
        return rendererSystem->RequestPick(screenX, screenY, viewportPos, viewportSize);
    }

    bool ViewportPicking::TryGetAsyncPickResult(uint32_t requestId, uint32_t& outEntityId, ECS::RendererSystem* rendererSystem)
    {
        if (!rendererSystem) return false;
        return rendererSystem->TryGetPickResult(requestId, outEntityId);
    }

}
