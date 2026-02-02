/* Start Header *****************************************************************/
/*!
\file    EventComponents.h
\author  Dalton Koh (100%)
\par     d.koh.b@digipen.edu
\brief
Definitions of event components used in the ECS event system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ECS_EVENTS_EVENTCOMPONENTS_H
#define ECS_EVENTS_EVENTCOMPONENTS_H

#include "math/Vector3D.h"
#include <cstdint>
#include "ecs/Entity.h"

namespace ECS::Events {
    static constexpr uint32_t kMaxEventBufferSize = 8;

    /**
     * @brief Event component representing a collision between two entities.
     * Added by the physics system when a collision occurs.
     * Automatically removed at the end of the frame.
     */
    struct CollisionEvent {
        PackedEntityId OtherEntityId; // Packed entity that collided with this entity
        Vector3D ContactPoint; // Contact point in world space
        Vector3D ContactNormal; // Contact normal (points from this entity toward other)
        Vector3D RelativeVelocity; // Relative velocity of the two entities at contact
        float ImpulseMagnitude; // Magnitude of the collision impulse
    };

    /**
     * @brief Event component representing a trigger overlap event.
     * Added by the physics system when a trigger overlaps another collider.
     * Automatically removed at the end of the frame.
     */
    struct TriggerEvent {
        PackedEntityId OtherEntityId; // Packed entity overlapping the trigger
        bool IsEnter; // True if this is the first frame of overlap
        bool IsActive; // True if still overlapping
    };

    /**
     * @brief Event component representing the end of a collision.
     * Added by the physics system when two colliding entities separate.
     * Automatically removed at the end of the frame.
     */
    struct CollisionExitEvent {
        PackedEntityId OtherEntityId; // Packed entity that stopped colliding
        Vector3D LastContactPoint; // Last contact point
    };

    /**
     * @brief Event component representing the end of a trigger overlap.
     * Added by the physics system when a trigger stops overlapping another collider.
     * Automatically removed at the end of the frame.
     */
    struct TriggerExitEvent {
        PackedEntityId OtherEntityId; // Packed entity that stopped overlapping
    };

    /**
     * @brief Per-entity buffer of collision events for the current frame.
     */
    struct CollisionEventBuffer {
        uint32_t Count = 0;
        CollisionEvent Events[kMaxEventBufferSize]{};
    };

    /**
     * @brief Per-entity buffer of trigger events for the current frame.
     */
    struct TriggerEventBuffer {
        uint32_t Count = 0;
        TriggerEvent Events[kMaxEventBufferSize]{};
    };

    /**
     * @brief Per-entity buffer of collision exit events for the current frame.
     */
    struct CollisionExitEventBuffer {
        uint32_t Count = 0;
        CollisionExitEvent Events[kMaxEventBufferSize]{};
    };

    /**
     * @brief Per-entity buffer of trigger exit events for the current frame.
     */
    struct TriggerExitEventBuffer {
        uint32_t Count = 0;
        TriggerExitEvent Events[kMaxEventBufferSize]{};
    };
}

#endif
