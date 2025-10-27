using System;
using GrapeEngine.Numerics;

namespace GrapeEngine.Scripting
{
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
            if (entity.TryGetComponent<LocalTransform>(out var transform))
            {
                transform.Position += delta;
                entity.SetComponent(transform);
            }
        }

        /// <summary>
        /// Set an entity's position.
        /// </summary>
        /// <param name="entity">The entity</param>
        /// <param name="position">The new position</param>
        public static void SetPosition(Entity entity, Vector3 position)
        {
            if (entity.TryGetComponent<LocalTransform>(out var transform))
            {
                transform.Position = position;
                entity.SetComponent(transform);
            }
        }

        /// <summary>
        /// Get an entity's position.
        /// </summary>
        /// <param name="entity">The entity</param>
        /// <returns>The position, or Vector3.Zero if the entity has no transform</returns>
        public static Vector3 GetPosition(Entity entity)
        {
            if (entity.TryGetComponent<LocalTransform>(out var transform))
                return transform.Position;
            return Vector3.Zero;
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
            if (entity.TryGetComponent<LocalTransform>(out var transform))
            {
                transform.Scale = scale;
                entity.SetComponent(transform);
            }
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
            if (entity.TryGetComponent<LocalTransform>(out var transform))
                return transform.Scale;
            return Vector3.One;
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
            if (entity.TryGetComponent<LocalTransform>(out var transform))
            {
                transform.Rotation = rotation;
                entity.SetComponent(transform);
            }
        }

        /// <summary>
        /// Get an entity's rotation.
        /// </summary>
        /// <param name="entity">The entity</param>
        /// <returns>The rotation, or Quaternion.Identity if the entity has no transform</returns>
        public static Quaternion GetRotation(Entity entity)
        {
            if (entity.TryGetComponent<LocalTransform>(out var transform))
                return transform.Rotation;
            return Quaternion.Identity;
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
            if (a.TryGetComponent<LocalTransform>(out var transformA) &&
                b.TryGetComponent<LocalTransform>(out var transformB))
            {
                return (transformA.Position - transformB.Position).Magnitude;
            }
            return float.MaxValue;
        }

        /// <summary>
        /// Calculate the direction vector from one entity to another.
        /// </summary>
        /// <param name="from">Source entity</param>
        /// <param name="to">Target entity</param>
        /// <returns>Normalized direction vector, or Vector3.Zero if either entity has no transform</returns>
        public static Vector3 DirectionTo(Entity from, Entity to)
        {
            if (from.TryGetComponent<LocalTransform>(out var transformFrom) &&
                to.TryGetComponent<LocalTransform>(out var transformTo))
            {
                Vector3 direction = transformTo.Position - transformFrom.Position;
                return direction.Normalized;
            }
            return Vector3.Zero;
        }

        /// <summary>
        /// Make an entity look at a target position (2D - rotates around Z axis).
        /// Note: This is a simplified 2D rotation.
        /// </summary>
        /// <param name="entity">The entity to rotate</param>
        /// <param name="target">The target position to look at</param>
        public static void LookAt2D(Entity entity, Vector3 target)
        {
            if (entity.TryGetComponent<LocalTransform>(out var transform))
            {
                Vector3 direction = target - transform.Position;
                float angle = MathF.Atan2(direction.Y, direction.X);
                
                // Convert to quaternion rotation around Z axis
                // This is a simplified 2D rotation - for full 3D, use proper quaternion math
                float halfAngle = angle * 0.5f;
                transform.Rotation = new Quaternion(
                    0,
                    0,
                    MathF.Sin(halfAngle),
                    MathF.Cos(halfAngle)
                );
                
                entity.SetComponent(transform);
            }
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
            if (entity.TryGetComponent<LocalTransform>(out var transform))
            {
                transform.Position = Lerp(transform.Position, target, t);
                entity.SetComponent(transform);
            }
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
            float t = 1.0f - MathF.Exp(-deltaTime / smoothTime);
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
            t = Math.Clamp(t, 0.0f, 1.0f);
            return new Vector3(
                a.X + (b.X - a.X) * t,
                a.Y + (b.Y - a.Y) * t,
                a.Z + (b.Z - a.Z) * t
            );
        }
    }
}
