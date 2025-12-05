#ifndef UI_EVENT_SYSTEM_H
#define UI_EVENT_SYSTEM_H

#include "ecs/World.h"
#include "services/UIEvents.h"
#include "Math/Vector2D.h"

namespace ECS {

    /**
     * @brief UI Event System - Generates UI events based on object picking
     *
     * Works exactly like PhysicsSystem/CollisionEvents:
     * - No component needed
     * - Uses picking system to detect which entity is under mouse
     * - Fills UIEventQueue with events
     * - Scripts poll UIEventQueue::GetEvents()
     *
     * Usage in game loop:
     *   1. UIEventQueue::Clear() at start of frame
     *   2. UIEventSystem::Update() to generate events
     *   3. Scripts poll UIEventQueue::GetEvents(myEntity)
     */
    class UIEventSystem {
    public:
        /**
         * @brief Initialize the UI event system
         */
        static void Initialize();

        /**
         * @brief Update UI events based on picked entity
         * @param world The ECS world (optional, for entity validation)
         * @param pickedEntityID Entity ID from picking system (0 or INVALID = no entity)
         * @param mouseScreenPos Mouse position in screen space
         */
        static void Update(World* world, uint32_t pickedEntityID, const Vector2D& mouseScreenPos);

        /**
         * @brief Get the currently hovered entity
         */
        static Entity GetHoveredEntity() { return s_hoveredEntity; }

        /**
         * @brief Check if mouse is over any UI element
         */
        static bool IsMouseOverUI() { return !s_hoveredEntity.IsNull(); }

    private:
        static Entity s_hoveredEntity;
        static Entity s_previousHoveredEntity;
    };

} // namespace ECS

#endif // UI_EVENT_SYSTEM_H
