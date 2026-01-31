/* Start Header *****************************************************************/
/*!
\file   CameraFrustumRenderer.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\brief
Editor utility to visualize game camera frustum in the scene viewport.
Shows a cyan rectangle indicating what the game camera will see.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "CameraFrustumRenderer.h"
#include "ecs/Components.h"
#include "EditorECSUtils.h"
#include "graphics/debugDraw2D.hpp"
#include "graphics/renderer.hpp"
#include "graphics/shader.hpp"
#include <glm/glm.hpp>

namespace Editor {

    void CameraFrustumRenderer::RenderFrustum(
        ECS::World& world,
        Renderer& renderer,
        Shader& shader,
        const glm::mat4& viewProj,
        float cameraOrthoSize,
        float windowHeight,
        uint32_t excludeEntityId)
    {
        shader.use();
        shader.setMat4("uViewProj", viewProj);
        renderer.beginFrame();

        const ECS::ComponentTypeId localTransformId = Editor::ECSUtils::GetComponentIdFromName("LocalTransform");
        const ECS::ComponentTypeId cameraId = Editor::ECSUtils::GetComponentIdFromName("Camera3D");

        if (localTransformId == ECS::NULL_COMPONENT_ID || cameraId == ECS::NULL_COMPONENT_ID) {
            renderer.endFrame();
            return;
        }

        // Find active game cameras (excluding editor camera if specified)
        world.Each([&](ECS::Entity entity) {
                if (!world.HasById(entity, localTransformId) || !world.HasById(entity, cameraId)) {
                    return;
                }

                const auto* camTransform = static_cast<const ECS::Components::LocalTransform*>(
                    world.GetRawComponentPtr(entity, localTransformId));
                const auto* camera = static_cast<const ECS::Components::Camera3D*>(
                    world.GetRawComponentPtr(entity, cameraId));

                if (!camTransform || !camera) {
                    return;
                }

                // Skip the excluded entity (typically the editor camera)
                if (excludeEntityId != 0 && entity.Index == excludeEntityId) {
                    return;
                }

                // Skip inactive cameras
                if (!camera->Active) {
                    return;
                }

                // Calculate camera frustum bounds in world space
                const float halfH = camera->OrthoSize * 0.5f;
                const float halfW = halfH * camera->AspectRatio;

                // Camera position in world space
                const glm::vec2 camPos(camTransform->Position.X, camTransform->Position.Y);

                // Frustum corners (centered on camera position)
                const glm::vec2 frustumMin = camPos - glm::vec2(halfW, halfH);
                const glm::vec2 frustumMax = camPos + glm::vec2(halfW, halfH);

                // Calculate constant screen-space thickness (always 2 pixels)
                const float desiredPixelThickness = 2.0f;
                const float worldThickness = (cameraOrthoSize / windowHeight) * desiredPixelThickness;

                // Draw frustum rectangle with cyan color
                const glm::vec4 frustumColor(0.0f, 1.0f, 1.0f, 0.6f); // Cyan, semi-transparent

                DebugDraw2D::RectStroke(renderer, frustumMin, frustumMax,
                    worldThickness, frustumColor, 0);
            });

        renderer.endFrame();
    }

}
