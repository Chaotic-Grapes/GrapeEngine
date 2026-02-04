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
#include "ecs/systems/RendererSystem.h"
#include "helpers/TransformUtils.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace Editor {

    void CameraFrustumRenderer::RenderFrustum(
        ECS::World& world,
        ECS::RendererSystem* rendererSystem,
        float cameraOrthoSize,
        float windowHeight,
        uint32_t excludeEntityId)
    {
        if (!rendererSystem) {
            return;
        }

        const ECS::ComponentTypeId localTransformId = Editor::ECSUtils::GetComponentIdFromName("LocalTransform");
        const ECS::ComponentTypeId worldTransformId = Editor::ECSUtils::GetComponentIdFromName("WorldTransform");
        const ECS::ComponentTypeId cameraId = Editor::ECSUtils::GetComponentIdFromName("Camera3D");

        if (localTransformId == ECS::NULL_COMPONENT_ID || cameraId == ECS::NULL_COMPONENT_ID) {
            return;
        }

        constexpr float kFrustumPlaneZ = 0.0f;

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

                Vector3D position = camTransform->Position;
                Quaternion rotation = camTransform->Rotation;
                if (worldTransformId != ECS::NULL_COMPONENT_ID && world.HasById(entity, worldTransformId)) {
                    const auto* wt = static_cast<const ECS::Components::WorldTransform*>(
                        world.GetRawComponentPtr(entity, worldTransformId));
                    if (wt) {
                        Vector3D scale;
                        TransformUtils::DecomposeTRS(wt->Matrix, position, rotation, scale);
                    }
                }

                // Skip the excluded entity (typically the editor camera)
                if (excludeEntityId != 0 && entity.Index == excludeEntityId) {
                    return;
                }

                // Skip inactive cameras
                if (!camera->Active) {
                    return;
                }

                // Build camera basis from rotation.
                Vector3D forward = rotation.Rotate(Vector3D{0.0f, 0.0f, -1.0f});
                Vector3D right = rotation.Rotate(Vector3D{1.0f, 0.0f, 0.0f});
                Vector3D up = rotation.Rotate(Vector3D{0.0f, 1.0f, 0.0f});
                forward.Normalize();
                right.Normalize();
                up.Normalize();

                auto intersectPlaneZ = [&](const Vector3D& origin, const Vector3D& direction, glm::vec2& out) -> bool {
                    if (std::abs(direction.Z) < 1e-6f) {
                        return false;
                    }
                    const float t = (kFrustumPlaneZ - origin.Z) / direction.Z;
                    if (t <= 0.0f) {
                        return false;
                    }
                    const Vector3D hit = origin + direction * t;
                    out = glm::vec2(hit.X, hit.Y);
                    return true;
                };

                glm::vec2 corners[4];
                bool valid = true;
                if (camera->UsePerspective) {
                    const float tanHalfFov = std::tan(glm::radians(camera->FOV * 0.5f));
                    const float halfW = tanHalfFov * camera->AspectRatio;
                    const float halfH = tanHalfFov;
                    const Vector3D dirs[4] = {
                        (right * -halfW) + (up * -halfH) + forward,
                        (right *  halfW) + (up * -halfH) + forward,
                        (right *  halfW) + (up *  halfH) + forward,
                        (right * -halfW) + (up *  halfH) + forward
                    };

                    for (int i = 0; i < 4; ++i) {
                        Vector3D dir = dirs[i].Normalized();
                        if (!intersectPlaneZ(position, dir, corners[i])) {
                            valid = false;
                            break;
                        }
                    }
                }
                else {
                    const float halfH = camera->OrthoSize;
                    const float halfW = halfH * camera->AspectRatio;
                    const Vector3D origins[4] = {
                        position + (right * -halfW) + (up * -halfH),
                        position + (right *  halfW) + (up * -halfH),
                        position + (right *  halfW) + (up *  halfH),
                        position + (right * -halfW) + (up *  halfH)
                    };

                    for (int i = 0; i < 4; ++i) {
                        if (!intersectPlaneZ(origins[i], forward, corners[i])) {
                            valid = false;
                            break;
                        }
                    }
                }

                if (!valid) {
                    return;
                }

                // Calculate constant screen-space thickness (always 2 pixels)
                const float desiredPixelThickness = 2.0f;
                const float safeHeight = std::max(windowHeight, 1.0f);
                const float worldThickness = (cameraOrthoSize / safeHeight) * desiredPixelThickness;

                // Draw frustum rectangle with cyan color
                const glm::vec4 frustumColor(0.0f, 1.0f, 1.0f, 0.6f); // Cyan, semi-transparent

                for (int i = 0; i < 4; ++i) {
                    const glm::vec2& a = corners[i];
                    const glm::vec2& b = corners[(i + 1) % 4];
                    rendererSystem->SubmitWireframeLine(a, b, frustumColor, worldThickness);
                }
            });
    }

}
