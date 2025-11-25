
#ifndef BOUNDARY_CHECK_SYSTEM_H
#define BOUNDARY_CHECK_SYSTEM_H

#include "ecs/World.h"
#include "Math/Vector2D.h"
#include <functional>

// Forward declarations for camera types
namespace Engine { class EditorCamera; }

namespace ECS {

    // Viewport information for coordinate transformation
    struct ViewportInfo {
        float MinX;           // Viewport left edge in screen coordinates
        float MinY;           // Viewport top edge in screen coordinates
        float Width;          // Viewport width in pixels
        float Height;         // Viewport height in pixels
    };

    // Event data for UI clicks
    struct UIClickEvent {

        // Object to be clicked
        Entity ClickedEntity;

        // For position tracking 
        Vector2D MousePosition;
        Vector2D EntityPosition;

        // MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE
        int MouseButton;

        // From UIClickable component
        uint32_t ActionID;
    };

    class BoundaryCheckSystem {
    public:

        // Initialize the system
        static void Initialize(World* world);

        // Update every frame - checks for mouse input and triggers UI events
        // Pass viewport bounds and camera for proper coordinate transformation
        static void Update(World* world, const ViewportInfo& viewport, Engine::EditorCamera* camera);


        // Register a callback for UI click events
        static void RegisterClickCallback(std::function<void(const UIClickEvent&)> callback);

        // Clear all registered callbacks
        static void ClearCallbacks();


        // Check if mouse is currently over any UI element
        static bool IsMouseOverUI();

        // Get the topmost UI entity under the mouse cursor (returns NULL_ENTITY if none)
        static Entity GetUIEntityUnderMouse();

        static bool WasAnyUIClicked();

        static uint32_t GetLastClickedActionID();
        static Entity GetLastClickedEntity();

        // Test if a point is inside an entity's bounds
        // Uses the entity's CircleCollider2D or BoxCollider2D
        static bool TestPointInEntity(World* world, Entity entity, const Vector2D& worldPoint);

        // Test if mouse is currently over a specific entity
        static bool IsMouseOverEntity(World* world, Entity entity);

    private:

        static bool s_anyClicked;

        // Convert screen coordinates to world coordinates using active camera
        static Vector2D ScreenToWorld(const Vector2D& screenPos, const ViewportInfo& viewport, Engine::EditorCamera* camera);


        // Test if point is inside a circle collider
        static bool PointInCircle(const Vector2D& point,
            const Vector2D& center,
            float radius,
            const Vector2D& offset);

        // Test if point is inside a box collider (AABB)
        static bool PointInBox(const Vector2D& point,
            const Vector2D& center,
            const Vector2D& size,
            const Vector2D& offset);


        // Stored click callbacks
        static std::vector<std::function<void(const UIClickEvent&)>> s_clickCallbacks;

        // Current frame state
        static bool s_mouseOverUI;
        static Entity s_hoveredEntity;

        static uint32_t s_lastClickedActionID;
        static Entity s_lastClickedEntity;


        // Previous frame state for edge detection
        static Entity s_previousHoveredEntity;
    };

} // namespace ECS

#endif // BOUNDARY_CHECK_SYSTEM_H