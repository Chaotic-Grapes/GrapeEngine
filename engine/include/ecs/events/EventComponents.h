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

namespace ECS::Events {
    /**
     * @brief Event component representing a collision between two entities.
     * Added by the physics system when a collision occurs.
     * Automatically removed at the end of the frame.
     */
    struct CollisionEvent {
        EntityId OtherEntityId; // Entity that collided with this entity
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
        EntityId OtherEntityId; // Entity that is overlapping with the trigger
        bool IsEnter; // True if this is the first frame of overlap
        bool IsActive; // True if still overlapping
    };

    /**
     * @brief Event component representing the end of a collision.
     * Added by the physics system when two colliding entities separate.
     * Automatically removed at the end of the frame.
     */
    struct CollisionExitEvent {
        EntityId OtherEntityId; // Entity that stopped colliding
        Vector3D LastContactPoint; // Last contact point
    };

    /**
     * @brief Event component representing the end of a trigger overlap.
     * Added by the physics system when a trigger stops overlapping another collider.
     * Automatically removed at the end of the frame.
     */
    struct TriggerExitEvent {
        EntityId OtherEntityId; // Entity that stopped overlapping
    };
}

#endif
