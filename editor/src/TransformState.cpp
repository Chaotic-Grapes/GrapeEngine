/* Start Header *****************************************************************/
/*!
\file   TransformState.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of transform state snapshot and delta structures.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "TransformState.h"
#include <cmath>

namespace Editor {

    /**
     * @brief Return true if this transform state is within epsilon of the other in position, rotation, and scale.
     * @param other Transform state to compare against.
     * @param epsilon Maximum allowable difference to still be considered equal.
     * @return True if all components are within epsilon, false otherwise.
     */
    bool CachedTransformState::Equals(const CachedTransformState& other, float epsilon) const {
        // Compare position component-wise
        Vector3D posDiff = Position - other.Position;
        float posDistance = std::sqrt(posDiff.X * posDiff.X + posDiff.Y * posDiff.Y + posDiff.Z * posDiff.Z);
        if (posDistance > epsilon) return false;

        // Compare rotation: check if quaternions are approximately equal or opposite
        float dotProd = Rotation.X * other.Rotation.X +
                       Rotation.Y * other.Rotation.Y +
                       Rotation.Z * other.Rotation.Z +
                       Rotation.W * other.Rotation.W;
        if (std::fabs(std::fabs(dotProd) - 1.0f) > epsilon) return false;

        // Compare scale component-wise
        Vector3D scaleDiff = Scale - other.Scale;
        float scaleDistance = std::sqrt(scaleDiff.X * scaleDiff.X + scaleDiff.Y * scaleDiff.Y + scaleDiff.Z * scaleDiff.Z);
        if (scaleDistance > epsilon) return false;

        return true;
    }

    /**
     * @brief Compute the positional, rotational, and scale delta from this state to the given final state.
     * @param final The target transform state.
     * @return TransformDelta representing the change from this state to final.
     */
    TransformDelta CachedTransformState::ComputeDelta(const CachedTransformState& final) const {
        // Compute position delta
        Vector3D posDelta = final.Position - this->Position;

        // Compute rotation delta as the relative rotation
        Quaternion rotDelta = final.Rotation * this->Rotation.Inverse();

        // Compute scale delta
        Vector3D scaleDelta = final.Scale - this->Scale;

        return TransformDelta(posDelta, rotDelta, scaleDelta);
    }

    /**
     * @brief Return a new transform state with the given delta applied to this one.
     * @param delta The delta to apply.
     * @return New CachedTransformState with the delta applied.
     */
    CachedTransformState CachedTransformState::ApplyDelta(const TransformDelta& delta) const {
        return delta.ApplyTo(*this);
    }

    /**
     * @brief Return true if position, rotation, and scale deltas are all within epsilon of zero/identity.
     * @param epsilon Maximum allowable magnitude to still be considered zero.
     * @return True if all deltas are effectively zero, false otherwise.
     */
    bool TransformDelta::IsZero(float epsilon) const {
        // Check position delta
        float posDistance = std::sqrt(PositionDelta.X * PositionDelta.X +
                                     PositionDelta.Y * PositionDelta.Y +
                                     PositionDelta.Z * PositionDelta.Z);
        if (posDistance > epsilon) return false;

        // Check rotation delta (identity quaternion has w=1, x=y=z=0)
        float angleError = std::fabs(1.0f - std::fabs(RotationDelta.W));
        if (angleError > epsilon) return false;

        // Check scale delta
        float scaleDistance = std::sqrt(ScaleDelta.X * ScaleDelta.X +
                                       ScaleDelta.Y * ScaleDelta.Y +
                                       ScaleDelta.Z * ScaleDelta.Z);
        if (scaleDistance > epsilon) return false;

        return true;
    }

    /**
     * @brief Return a new delta that is the inverse of this one, suitable for undoing a transform change.
     * @return Inverted TransformDelta with negated position, scale, and conjugated rotation.
     */
    TransformDelta TransformDelta::Inverted() const {
        // Negate position and scale deltas
        Vector3D invertedPosDelta(PositionDelta.X * -1.0f,
                                 PositionDelta.Y * -1.0f,
                                 PositionDelta.Z * -1.0f);
        
        Vector3D invertedScaleDelta(ScaleDelta.X * -1.0f,
                                   ScaleDelta.Y * -1.0f,
                                   ScaleDelta.Z * -1.0f);

        // Invert rotation delta
        Quaternion invertedRotDelta(RotationDelta.X * -1.0f,
                                   RotationDelta.Y * -1.0f,
                                   RotationDelta.Z * -1.0f,
                                   RotationDelta.W);

        return TransformDelta(invertedPosDelta, invertedRotDelta, invertedScaleDelta);
    }

    /**
     * @brief Add this delta's position and scale offsets and compose its rotation onto the given state.
     * @param state The base transform state to apply this delta to.
     * @return New CachedTransformState with this delta applied.
     */
    CachedTransformState TransformDelta::ApplyTo(const CachedTransformState& state) const {
        // Apply position delta
        Vector3D newPosition(state.Position.X + this->PositionDelta.X,
                           state.Position.Y + this->PositionDelta.Y,
                           state.Position.Z + this->PositionDelta.Z);

        // Apply rotation delta
        Quaternion newRotation = state.Rotation * this->RotationDelta;

        // Apply scale delta
        Vector3D newScale(state.Scale.X + this->ScaleDelta.X,
                         state.Scale.Y + this->ScaleDelta.Y,
                         state.Scale.Z + this->ScaleDelta.Z);

        return CachedTransformState(newPosition, newRotation, newScale);
    }

}  // namespace Editor
