/* Start Header *****************************************************************/
/*!
\file   TransformState.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Transform state structures for capture, comparison, and undo/redo operations.

These are lightweight POD types with zero external dependencies used throughout
the gizmo and picking systems to represent entity transforms consistently.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef TRANSFORM_STATE_H
#define TRANSFORM_STATE_H

#include "math/Vector3D.h"
#include "math/Quaternion.h"
#include <glm/glm.hpp>

namespace Editor {

    /**
     * @brief Lightweight snapshot of an entity's local transform
     * 
     * Used to capture the state of a transform at discrete points in time
     * (e.g. start of drag, end of drag) for comparison and undo/redo.
     * 
     * This is a simple POD type with no methods beyond accessors.
     */
    struct CachedTransformState {
        Vector3D Position{0.0f, 0.0f, 0.0f};
        Quaternion Rotation{0.0f, 0.0f, 0.0f, 1.0f};
        Vector3D Scale{1.0f, 1.0f, 1.0f};

        // Default constructor and destructor are trivial
        CachedTransformState() = default;
        ~CachedTransformState() = default;

        /**
         * @brief Construct from individual components
         */
        CachedTransformState(const Vector3D& pos, const Quaternion& rot, const Vector3D& scl)
            : Position(pos), Rotation(rot), Scale(scl) {}

        /**
         * @brief Check if this state is equal to another (component-wise)
         * Uses approximate floating-point comparison for rotation/scale
         */
        bool Equals(const CachedTransformState& other, float epsilon = 0.0001f) const;

        /**
         * @brief Compute the delta (change) from this state to another
         * @param final The final state
         * @return TransformDelta representing the change
         */
        struct TransformDelta ComputeDelta(const CachedTransformState& final) const;

        /**
         * @brief Apply a delta to this state and return the result
         * @param delta The change to apply
         * @return New state with delta applied
         */
        CachedTransformState ApplyDelta(const TransformDelta& delta) const;
    };

    /**
     * @brief Represents a transformation change (delta) from one state to another
     * 
     * Stores the differences in position, rotation, and scale between two
     * transform states. Can be applied or inverted for undo/redo operations.
     * 
     * This is a simple POD type with no methods beyond basic operations.
     */
    struct TransformDelta {
        Vector3D PositionDelta{0.0f, 0.0f, 0.0f};
        Quaternion RotationDelta{0.0f, 0.0f, 0.0f, 1.0f};  // Identity quaternion (no rotation change)
        Vector3D ScaleDelta{0.0f, 0.0f, 0.0f};

        // Default constructor and destructor are trivial
        TransformDelta() = default;
        ~TransformDelta() = default;

        /**
         * @brief Construct from individual component changes
         */
        TransformDelta(const Vector3D& posDelta, const Quaternion& rotDelta, const Vector3D& scaleDelta)
            : PositionDelta(posDelta), RotationDelta(rotDelta), ScaleDelta(scaleDelta) {}

        /**
         * @brief Check if this delta is zero (no change)
         */
        bool IsZero(float epsilon = 0.0001f) const;

        /**
         * @brief Invert this delta for undo operations
         * @return Negated delta that can be applied to undo the change
         */
        TransformDelta Inverted() const;

        /**
         * @brief Apply this delta to a state
         * @param state The initial state
         * @return New state with delta applied
         */
        CachedTransformState ApplyTo(const CachedTransformState& state) const;
    };

}  // namespace Editor

#endif  // TRANSFORM_STATE_H
