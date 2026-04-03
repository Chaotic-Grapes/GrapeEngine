/* Start Header *****************************************************************/
/*!
\file   CameraFrustumRenderer.cpp
\author Choi Meng Yew (100%)
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

    /**
     * @brief Draw world-space wireframe frustum rectangles for all active scene cameras.
     * @param world ECS world to query for Camera3D and transform components.
     * @param rendererSystem Renderer system used to submit wireframe line draws.
     * @param cameraOrthoSize OrthoSize of the editor camera, used to compute world-space line thickness.
     * @param windowHeight Height of the window in pixels, used to compute world-space line thickness.
     * @param excludeEntityId Entity index to skip (typically the editor camera); pass 0 to include all.
     */
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

        // Resolve component type IDs once per call — avoids repeated string lookups inside the loop
        const ECS::ComponentTypeId localTransformId = Editor::ECSUtils::GetComponentIdFromName("LocalTransform");
        const ECS::ComponentTypeId worldTransformId = Editor::ECSUtils::GetComponentIdFromName("WorldTransform");
        const ECS::ComponentTypeId cameraId = Editor::ECSUtils::GetComponentIdFromName("Camera3D");

        // Both LocalTransform and Camera3D must be registered; WorldTransform is optional
        if (localTransformId == ECS::NULL_COMPONENT_ID || cameraId == ECS::NULL_COMPONENT_ID) {
            return;
        }

        // The frustum is visualized as a cross-section at Z = 0 (the game world's ground plane)
        constexpr float kFrustumPlaneZ = 0.0f;

        // Iterate every entity and filter down to active cameras
        world.Each([&](ECS::Entity entity) {
                // Only process entities that have both a transform and a camera component
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

                // Start from local transform; upgrade to world transform if available
                // (entities parented to another object need the world-space pose)
                Vector3D position = camTransform->Position;
                Quaternion rotation = camTransform->Rotation;
                if (worldTransformId != ECS::NULL_COMPONENT_ID && world.HasById(entity, worldTransformId)) {
                    const auto* wt = static_cast<const ECS::Components::WorldTransform*>(
                        world.GetRawComponentPtr(entity, worldTransformId));
                    if (wt) {
                        // Decompose the world matrix to extract position and rotation
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

                // Build orthonormal camera basis vectors from the camera's rotation quaternion
                // forward = -Z, right = +X, up = +Y in camera-local space
                Vector3D forward = rotation.Rotate(Vector3D{0.0f, 0.0f, -1.0f});
                Vector3D right = rotation.Rotate(Vector3D{1.0f, 0.0f, 0.0f});
                Vector3D up = rotation.Rotate(Vector3D{0.0f, 1.0f, 0.0f});
                forward.Normalize();
                right.Normalize();
                up.Normalize();

                // Ray-plane intersection helper: find where a ray from 'origin' along 'direction' hits Z=0.
                // Returns false if the ray is nearly parallel to the plane or points away from it.
                auto intersectPlaneZ = [&](const Vector3D& origin, const Vector3D& direction, glm::vec2& out) -> bool {
                    if (std::abs(direction.Z) < 1e-6f) {
                        // Ray is parallel to the XY plane — no intersection
                        return false;
                    }
                    const float t = (kFrustumPlaneZ - origin.Z) / direction.Z;
                    if (t <= 0.0f) {
                        // Intersection is behind the ray origin
                        return false;
                    }
                    const Vector3D hit = origin + direction * t;
                    out = glm::vec2(hit.X, hit.Y);
                    return true;
                };

                glm::vec2 corners[4];
                bool valid = true;
                if (camera->UsePerspective) {
                    // Perspective frustum: project the four near-plane corner directions to Z=0.
                    // halfW and halfH are the half-extents at unit distance from the camera,
                    // derived from tan(FOV/2) and the camera's aspect ratio.
                    const float tanHalfFov = std::tan(glm::radians(camera->FOV * 0.5f));
                    const float halfW = tanHalfFov * camera->AspectRatio;
                    const float halfH = tanHalfFov;

                    // Corner directions in world space (bottom-left, bottom-right, top-right, top-left)
                    const Vector3D dirs[4] = {
                        (right * -halfW) + (up * -halfH) + forward,
                        (right *  halfW) + (up * -halfH) + forward,
                        (right *  halfW) + (up *  halfH) + forward,
                        (right * -halfW) + (up *  halfH) + forward
                    };

                    for (int i = 0; i < 4; ++i) {
                        // Normalize to get a proper ray direction before intersecting
                        Vector3D dir = dirs[i].Normalized();
                        if (!intersectPlaneZ(position, dir, corners[i])) {
                            valid = false;
                            break;
                        }
                    }
                }
                else {
                    // Orthographic frustum: rays are parallel to forward, one per corner.
                    // OrthoSize is the half-height; halfW scales it by aspect ratio.
                    const float halfH = camera->OrthoSize;
                    const float halfW = halfH * camera->AspectRatio;

                    // Corner ray origins (shifted from camera position in right/up directions)
                    const Vector3D origins[4] = {
                        position + (right * -halfW) + (up * -halfH),
                        position + (right *  halfW) + (up * -halfH),
                        position + (right *  halfW) + (up *  halfH),
                        position + (right * -halfW) + (up *  halfH)
                    };

                    for (int i = 0; i < 4; ++i) {
                        // All rays point in the same forward direction for an ortho camera
                        if (!intersectPlaneZ(origins[i], forward, corners[i])) {
                            valid = false;
                            break;
                        }
                    }
                }

                // If any corner ray missed the plane (e.g. camera looking upward), skip this camera
                if (!valid) {
                    return;
                }

                // Convert desired pixel thickness to world-space thickness using the editor camera's scale.
                // worldThickness = (orthoSize / windowHeight) * pixelCount — keeps the line a fixed 2px on screen
                const float desiredPixelThickness = 2.0f;
                const float safeHeight = std::max(windowHeight, 1.0f);
                const float worldThickness = (cameraOrthoSize / safeHeight) * desiredPixelThickness;

                // Draw the four edges of the frustum rectangle in cyan (semi-transparent)
                const glm::vec4 frustumColor(0.0f, 1.0f, 1.0f, 0.6f);

                for (int i = 0; i < 4; ++i) {
                    const glm::vec2& a = corners[i];
                    const glm::vec2& b = corners[(i + 1) % 4]; // Wrap last corner back to first
                    rendererSystem->SubmitWireframeLine(a, b, frustumColor, worldThickness);
                }
            });
    }

}
