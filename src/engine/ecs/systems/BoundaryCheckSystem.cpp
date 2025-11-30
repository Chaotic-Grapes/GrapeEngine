
//#include "ecs/systems/BoundaryCheckSystem.h"
//#include "ecs/Components.h"
////#include "ecs/systems/BoundaryCheckSystem.h"
//#include "ecs/Components.h"
//#include "../engine/services/Input.h"
//#include "graphics/EditorCamera.hpp"
//#include <algorithm>
//#include <cmath>
//#include <glm/glm.hpp>
//#include <iostream>
//
//namespace ECS {
//
//    // =========================================================================
//    // Static Member Initialization
//    // =========================================================================
//
//    std::vector<std::function<void(const UIClickEvent&)>> BoundaryCheckSystem::s_clickCallbacks;
//    bool BoundaryCheckSystem::s_mouseOverUI = false;
//    Entity BoundaryCheckSystem::s_hoveredEntity = NULL_ENTITY;
//    Entity BoundaryCheckSystem::s_previousHoveredEntity = NULL_ENTITY;
//    bool BoundaryCheckSystem::s_anyClicked = false;
//    uint32_t BoundaryCheckSystem::s_lastClickedActionID = 0;
//    Entity BoundaryCheckSystem::s_lastClickedEntity = NULL_ENTITY;
//
//    // =========================================================================
//    // Lifecycle
//    // =========================================================================
//
//    void BoundaryCheckSystem::Initialize(World* world) {
//        s_clickCallbacks.clear();
//        s_mouseOverUI = false;
//        s_hoveredEntity = NULL_ENTITY;
//        s_previousHoveredEntity = NULL_ENTITY;
//    }
//
//    void BoundaryCheckSystem::Update(World* world, const ViewportInfo& viewport, Engine::EditorCamera* camera) {
//        if (!world) return;
//
//        // reset for every click
//        s_anyClicked = false;
//
//        // Callback entity clicked tracking
//        s_lastClickedActionID = 0;       
//        s_lastClickedEntity = NULL_ENTITY;
//
//        // Get mouse position in screen space
//        double mouseX, mouseY;
//        Input::GetMousePosition(mouseX, mouseY);
//        Vector2D screenMouse(static_cast<float>(mouseX), static_cast<float>(mouseY));
//
//        // Convert to world space
//        Vector2D worldMouse = ScreenToWorld(screenMouse, viewport, camera);
//
//        // Track previous hover state
//        s_previousHoveredEntity = s_hoveredEntity;
//
//        // Collect all UI entities with their Z-order for sorting
//        struct UICandidate {
//            Entity entity;
//            int16_t zOrder;
//            Vector2D position;
//        };
//        std::vector<UICandidate> candidates;
//
//        // Query all entities with UIClickable component
//        world->Each<Components::UIClickable, Components::LocalTransform>(
//            [&](Entity entity,
//                Components::UIClickable& clickable,
//                Components::LocalTransform& transform) {
//
//                    // Skip if not enabled
//                    if (!clickable.Enabled) return;
//
//                    // Check if entity is active
//                    if (auto* active = world->TryGet<Components::Active>(entity)) {
//                        if (!active->Enabled) return;
//                    }
//
//                    // Must have either CircleCollider2D or BoxCollider2D
//                    bool hasCollider = world->Has<Components::CircleCollider2D>(entity) ||
//                        world->Has<Components::BoxCollider2D>(entity);
//                    if (!hasCollider) return;
//
//                    // Get Z-order for sorting (default to 0)
//                    int16_t zOrder = 0;
//                    if (auto* zIndex = world->TryGet<Components::ZIndex2D>(entity)) {
//                        zOrder = zIndex->ZOrder;
//                    }
//
//                    Vector2D entityPos(transform.Position.X, transform.Position.Y);
//                    candidates.push_back({ entity, zOrder, entityPos });
//            });
//
//        // Sort by Z-order (higher values = on top = checked first)
//        std::sort(candidates.begin(), candidates.end(),
//            [](const UICandidate& a, const UICandidate& b) {
//                return a.zOrder > b.zOrder;
//            });
//
//        // Reset all hover and click states
//        world->Each<Components::UIClickable>(
//            [](Entity entity, Components::UIClickable& clickable) {
//                (void)entity;  // Suppress unused parameter warning
//
//                clickable.IsHovered = false;
//                clickable.WasClicked = false;
//            });
//
//        // Find topmost entity under mouse
//        Entity hitEntity = NULL_ENTITY;
//    
//        for (const auto& candidate : candidates) {
//            auto* clickable = world->TryGet<Components::UIClickable>(candidate.entity);
//            if (!clickable) continue;
//
//            // Test if mouse is over this entity using its collider
//            if (TestPointInEntity(world, candidate.entity, worldMouse)) {
//                hitEntity = candidate.entity;
//                clickable->IsHovered = true;
//
//                // Check for mouse click
//                if (Input::IsMousePressed(MOUSE_LEFT)) {
//                    clickable->WasClicked = true;
//                    s_anyClicked = true;
//                    s_lastClickedActionID = clickable->ClickActionID;  
//                    s_lastClickedEntity = candidate.entity;           
//
//                    // Fire click callbacks
//                    UIClickEvent event;
//                    event.ClickedEntity = candidate.entity;
//                    event.MousePosition = worldMouse;
//                    event.EntityPosition = candidate.position;
//                    event.MouseButton = MOUSE_LEFT;
//                    event.ActionID = clickable->ClickActionID;
//
//                    for (auto& callback : s_clickCallbacks) {
//                        callback(event);
//                    }
//                }
//                // Only check topmost entity
//                break;
//            }
//        }
//
//        // Update state
//        s_hoveredEntity = hitEntity;
//        s_mouseOverUI = !hitEntity.IsNull();
//    }
//
//    // =========================================================================
//    // Callbacks
//    // =========================================================================
//
//    void BoundaryCheckSystem::RegisterClickCallback(std::function<void(const UIClickEvent&)> callback) {
//        s_clickCallbacks.push_back(callback);
//    }
//
//    void BoundaryCheckSystem::ClearCallbacks() {
//        s_clickCallbacks.clear();
//    }
//
//    uint32_t BoundaryCheckSystem::GetLastClickedActionID() {
//        return s_lastClickedActionID;
//    }
//
//    Entity BoundaryCheckSystem::GetLastClickedEntity() {
//        return s_lastClickedEntity;
//    }
//
//    // =========================================================================
//    // Query Functions
//    // =========================================================================
//
//    bool BoundaryCheckSystem::IsMouseOverUI() {
//        return s_mouseOverUI;
//    }
//
//    Entity BoundaryCheckSystem::GetUIEntityUnderMouse() {
//        return s_hoveredEntity;
//    }
//
//    bool BoundaryCheckSystem::IsMouseOverEntity(World* world, Entity entity) {
//        // NOTE: This function signature can't be changed without breaking compatibility
//        // For now, this won't work properly without viewport/camera info
//        // Consider deprecating this in favor of the Update() function doing all the work
//        return false;
//    }
//
//    bool BoundaryCheckSystem::WasAnyUIClicked() {
//        return s_anyClicked;
//    }
//
//    // =========================================================================
//    // Boundary Testing
//    // =========================================================================
//
//    bool BoundaryCheckSystem::TestPointInEntity(World* world, Entity entity, const Vector2D& worldPoint) {
//        auto* transform = world->TryGet<Components::LocalTransform>(entity);
//        if (!transform) return false;
//
//        Vector2D entityPos(transform->Position.X, transform->Position.Y);
//
//        std::cout << "[DEBUG] Transform Position: (" << transform->Position.X << ", " << transform->Position.Y << ")\n";
//        std::cout << "[DEBUG] Transform Scale: (" << transform->Scale.X << ", " << transform->Scale.Y << ")\n";
//        std::cout << "[DEBUG] Mouse Point: (" << worldPoint.X << ", " << worldPoint.Y << ")\n";
//
//        if (auto* circle = world->TryGet<Components::CircleCollider2D>(entity)) {
//            std::cout << "[DEBUG] Circle Radius: " << circle->Radius << "\n";
//            std::cout << "[DEBUG] Circle Offset: (" << circle->Offset.X << ", " << circle->Offset.Y << ")\n";
//
//            // APPLY SCALE TO RADIUS!
//            float scaledRadius = circle->Radius * ((transform->Scale.X + transform->Scale.Y) * 0.5f);
//            std::cout << "[DEBUG] Scaled Radius: " << scaledRadius << "\n";
//
//            return PointInCircle(worldPoint, entityPos, scaledRadius, circle->Offset);
//        }
//
//        if (auto* box = world->TryGet<Components::BoxCollider2D>(entity)) {
//            std::cout << "[DEBUG] Box HalfExtents: (" << box->HalfExtents.X << ", " << box->HalfExtents.Y << ")\n";
//            std::cout << "[DEBUG] Box Offset: (" << box->Offset.X << ", " << box->Offset.Y << ")\n";
//
//            // APPLY SCALE TO HALF EXTENTS!
//            Vector2D scaledHalfExtents(
//                box->HalfExtents.X * transform->Scale.X,
//                box->HalfExtents.Y * transform->Scale.Y
//            );
//            std::cout << "[DEBUG] Scaled HalfExtents: (" << scaledHalfExtents.X << ", " << scaledHalfExtents.Y << ")\n";
//
//            Vector2D fullSize(scaledHalfExtents.X * 2.0f, scaledHalfExtents.Y * 2.0f);
//            std::cout << "[DEBUG] Full Size: (" << fullSize.X << ", " << fullSize.Y << ")\n";
//
//            bool result = PointInBox(worldPoint, entityPos, fullSize, box->Offset);
//            std::cout << "[DEBUG] Hit Result: " << (result ? "HIT" : "MISS") << "\n\n";
//
//            return result;
//        }
//
//        return false;
//    }
//
//    // =========================================================================
//    // Coordinate Conversion
//    // =========================================================================
//
//    Vector2D BoundaryCheckSystem::ScreenToWorld(const Vector2D& screenPos, const ViewportInfo& viewport, Engine::EditorCamera* camera) {
//        if (!camera) {
//            // Fallback: return screen coordinates if no camera available
//            return screenPos;
//        }
//
//        // Step 1: Convert screen position to viewport-relative coordinates
//        float viewportX = screenPos.X - viewport.MinX;
//        float viewportY = screenPos.Y - viewport.MinY;
//
//        // Step 2: Normalize to 0-1 range within viewport
//        float normalizedX = viewportX / viewport.Width;
//        float normalizedY = viewportY / viewport.Height;
//
//        // Step 3: Convert to NDC (-1 to 1 range)
//        // Note: Screen Y increases downward, but NDC Y increases upward
//        float ndcX = normalizedX * 2.0f - 1.0f;
//        float ndcY = 1.0f - normalizedY * 2.0f;  // Flip Y axis
//
//        // Step 4: Get camera matrices
//        glm::mat4 projection = camera->GetProjectionMatrix();
//        glm::mat4 view = camera->GetViewMatrix();
//
//        // Calculate inverse view-projection matrix
//        glm::mat4 viewProj = projection * view;
//        glm::mat4 invViewProj = glm::inverse(viewProj);
//
//        // Step 5: Transform NDC point to world space
//        // For orthographic 2D, we want the Z coordinate to be at the camera's focal plane (z=0)
//        glm::vec4 ndcPos(ndcX, ndcY, 0.0f, 1.0f);
//        glm::vec4 worldPos = invViewProj * ndcPos;
//
//        // Perspective divide (not needed for orthographic, but safe to do)
//        if (worldPos.w != 0.0f) {
//            worldPos /= worldPos.w;
//        }
//
//        // Return 2D world position
//        return Vector2D(worldPos.x, worldPos.y);
//    }
//
//    // =========================================================================
//    // Collision Testing Helpers
//    // =========================================================================
//
//    bool BoundaryCheckSystem::PointInCircle(const Vector2D& point,
//        const Vector2D& center,
//        float radius,
//        const Vector2D& offset) {
//        // Calculate actual center with offset
//        Vector2D actualCenter = center + offset;
//
//        // Calculate distance squared (avoid sqrt for performance)
//        Vector2D delta = point - actualCenter;
//        float distSquared = delta.X * delta.X + delta.Y * delta.Y;
//        float radiusSquared = radius * radius;
//
//        return distSquared <= radiusSquared;
//    }
//
//    bool BoundaryCheckSystem::PointInBox(const Vector2D& point,
//        const Vector2D& center,
//        const Vector2D& size,
//        const Vector2D& offset) {
//        // Calculate actual center with offset
//        Vector2D actualCenter = center + offset;
//
//        // Calculate half extents
//        Vector2D halfSize = size * 0.5f;
//
//        // AABB test
//        return (point.X >= actualCenter.X - halfSize.X &&
//            point.X <= actualCenter.X + halfSize.X &&
//            point.Y >= actualCenter.Y - halfSize.Y &&
//            point.Y <= actualCenter.Y + halfSize.Y);
//    }
//
//} // namespace ECS