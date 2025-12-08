/* Start Header *****************************************************************/
/*!
\file   Transform.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   30th October 2025
\brief
Defines the Transform helper class for manipulating entity transforms.

\details
This static class provides convenient methods for working with LocalTransform
components of entities, including position, rotation, and scale operations.

\code
Transform.Translate(entity, new Vector3(1, 0, 0));
Transform.SetPosition(entity, new Vector3(0, 5, 0));

Vector3 pos = Transform.GetPosition(entity);

Transform.SetScale(entity, 2.0f);

Quaternion rot = Transform.GetRotation(entity);

float dist = Transform.Distance(entityA, entityB);

Vector3 dir = Transform.DirectionTo(entityA, entityB);

Transform.LookAt2D(entity, new Vector3(10, 10, 0));
Transform.LerpPosition(entity, new Vector3(5, 5, 0), 0.1f);
Transform.SmoothMove(entity, new Vector3(0, 0, 0), 0.5f, deltaTime);
\endcode

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Numerics;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Components.Core;

namespace GrapeEngine.Scripting.Helpers;

/// <summary>
/// Helper class for transform operations.
/// Provides convenient methods for working with LocalTransform components.
/// </summary>
public static class Transform
{
    // ============================================================================
    // Position Helpers
    // ============================================================================

    /// <summary>
    /// Move an entity's position by a delta.
    /// </summary>
    /// <param name="entity">The entity to move</param>
    /// <param name="delta">The position delta</param>
    public static void Translate(Entity entity, Vector3 delta)
    {
        if (!entity.HasComponent<LocalTransform>())
            return;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        transform.Position += delta;
    }

    /// <summary>
    /// Set an entity's position.
    /// </summary>
    /// <param name="entity">The entity</param>
    /// <param name="position">The new position</param>
    public static void SetPosition(Entity entity, Vector3 position)
    {
        if (!entity.HasComponent<LocalTransform>())
            return;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        transform.Position = position;
    }

    /// <summary>
    /// Get an entity's position.
    /// </summary>
    /// <param name="entity">The entity</param>
    /// <returns>The position, or Vector3.Zero if the entity has no transform</returns>
    public static Vector3 GetPosition(Entity entity)
    {
        if (!entity.HasComponent<LocalTransform>())
            return Vector3.Zero;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        return transform.Position;
    }

    // ============================================================================
    // Scale Helpers
    // ============================================================================

    /// <summary>
    /// Set an entity's scale.
    /// </summary>
    /// <param name="entity">The entity</param>
    /// <param name="scale">The new scale</param>
    public static void SetScale(Entity entity, Vector3 scale)
    {
        if (!entity.HasComponent<LocalTransform>())
            return;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        transform.Scale = scale;
    }

    /// <summary>
    /// Set uniform scale (all axes).
    /// </summary>
    /// <param name="entity">The entity</param>
    /// <param name="uniformScale">The uniform scale value</param>
    public static void SetScale(Entity entity, float uniformScale)
    {
        SetScale(entity, new Vector3(uniformScale, uniformScale, uniformScale));
    }

    /// <summary>
    /// Get an entity's scale.
    /// </summary>
    /// <param name="entity">The entity</param>
    /// <returns>The scale, or Vector3.One if the entity has no transform</returns>
    public static Vector3 GetScale(Entity entity)
    {
        if (!entity.HasComponent<LocalTransform>())
            return Vector3.One;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        return transform.Scale;
    }

    // ============================================================================
    // Rotation Helpers
    // ============================================================================

    /// <summary>
    /// Set an entity's rotation.
    /// </summary>
    /// <param name="entity">The entity</param>
    /// <param name="rotation">The new rotation quaternion</param>
    public static void SetRotation(Entity entity, Quaternion rotation)
    {
        if (!entity.HasComponent<LocalTransform>())
            return;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        transform.Rotation = rotation;
    }

    /// <summary>
    /// Get an entity's rotation.
    /// </summary>
    /// <param name="entity">The entity</param>
    /// <returns>The rotation, or Quaternion.Identity if the entity has no transform</returns>
    public static Quaternion GetRotation(Entity entity)
    {
        if (!entity.HasComponent<LocalTransform>())
            return Quaternion.Identity;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        return transform.Rotation;
    }

    // ============================================================================
    // Distance and Direction
    // ============================================================================

    /// <summary>
    /// Calculate the distance between two entities.
    /// </summary>
    /// <param name="a">First entity</param>
    /// <param name="b">Second entity</param>
    /// <returns>The distance, or float.MaxValue if either entity has no transform</returns>
    public static float Distance(Entity a, Entity b)
    {
        if (!a.HasComponent<LocalTransform>() || !b.HasComponent<LocalTransform>())
            return float.MaxValue;

        ref var transformA = ref a.GetComponent<LocalTransform>();
        ref var transformB = ref b.GetComponent<LocalTransform>();
        return (transformA.Position - transformB.Position).Magnitude;
    }

    /// <summary>
    /// Calculate the direction vector from one entity to another.
    /// </summary>
    /// <param name="from">Source entity</param>
    /// <param name="to">Target entity</param>
    /// <returns>Normalized direction vector, or Vector3.Zero if either entity has no transform</returns>
    public static Vector3 DirectionTo(Entity from, Entity to)
    {
        if (!from.HasComponent<LocalTransform>() || !to.HasComponent<LocalTransform>())
            return Vector3.Zero;

        ref var transformFrom = ref from.GetComponent<LocalTransform>();
        ref var transformTo = ref to.GetComponent<LocalTransform>();
        var direction = transformTo.Position - transformFrom.Position;
        return direction.Normalized;
    }

    /// <summary>
    /// Make an entity look at a target position (2D - rotates around Z axis).
    /// Note: This is a simplified 2D rotation.
    /// </summary>
    /// <param name="entity">The entity to rotate</param>
    /// <param name="target">The target position to look at</param>
    public static void LookAt2D(Entity entity, Vector3 target)
    {
        if (!entity.HasComponent<LocalTransform>())
            return;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        var direction = target - transform.Position;
        var angle = MathF.Atan2(direction.Y, direction.X);
            
        // Convert to quaternion rotation around Z axis
        // This is a simplified 2D rotation - for full 3D, use proper quaternion math
        var halfAngle = angle * 0.5f;
        transform.Rotation = new Quaternion(
            0,
            0,
            MathF.Sin(halfAngle),
            MathF.Cos(halfAngle)
        );
    }

    // ============================================================================
    // Interpolation
    // ============================================================================

    /// <summary>
    /// Linearly interpolate an entity's position towards a target.
    /// </summary>
    /// <param name="entity">The entity to move</param>
    /// <param name="target">The target position</param>
    /// <param name="t">Interpolation factor (0 to 1)</param>
    public static void LerpPosition(Entity entity, Vector3 target, float t)
    {
        if (!entity.HasComponent<LocalTransform>())
            return;

        ref var transform = ref entity.GetComponent<LocalTransform>();
        transform.Position = Lerp(transform.Position, target, t);
    }

    /// <summary>
    /// Smoothly interpolate an entity's position towards a target.
    /// </summary>
    /// <param name="entity">The entity to move</param>
    /// <param name="target">The target position</param>
    /// <param name="smoothTime">Approximate time to reach target</param>
    /// <param name="deltaTime">Time since last frame</param>
    public static void SmoothMove(Entity entity, Vector3 target, float smoothTime, float deltaTime)
    {
        if (smoothTime <= 0.0f) smoothTime = 0.0001f;
        var t = 1.0f - MathF.Exp(-deltaTime / smoothTime);
        LerpPosition(entity, target, t);
    }

    // ============================================================================
    // Utility
    // ============================================================================

    /// <summary>
    /// Linear interpolation between two vectors.
    /// </summary>
    private static Vector3 Lerp(Vector3 a, Vector3 b, float t)
    {
        t = GMath.Clamp(t, 0.0f, 1.0f);
        return new Vector3(
            a.X + (b.X - a.X) * t,
            a.Y + (b.Y - a.Y) * t,
            a.Z + (b.Z - a.Z) * t
        );
    }
}
