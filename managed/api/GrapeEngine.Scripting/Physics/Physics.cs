/* Start Header *****************************************************************/
/*!
\file   Physics.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
Provides access to 2D physics functionality.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Physics;

/// <summary>
/// Provides access to 2D physics functionality.
/// </summary>
public static class Physics
{
    // ============================================================================
    // World Settings
    // ============================================================================

    /// <summary>
    /// Get or set the global gravity vector.
    /// </summary>
    public static Vector2 Gravity
    {
        get
        {
            PhysicsAPI.GetGravity(out float x, out float y);
            return new Vector2(x, y);
        }
        set => PhysicsAPI.SetGravity(value.X, value.Y);
    }

    /// <summary>
    /// Get or set whether physics simulation is enabled.
    /// </summary>
    public static bool Enabled
    {
        get => PhysicsAPI.IsEnabled();
        set => PhysicsAPI.SetEnabled(value);
    }

    // ============================================================================
    // Forces and Velocities
    // ============================================================================

    /// <summary>
    /// Apply a force to an entity's rigidbody.
    /// </summary>
    /// <param name="entity">The entity to apply force to.</param>
    /// <param name="force">The force vector to apply.</param>
    public static void ApplyForce(Entity entity, Vector2 force)
        => PhysicsAPI.ApplyForce(entity.EntityId, force.X, force.Y);

    /// <summary>
    /// Apply an impulse (instant velocity change) to an entity's rigidbody.
    /// </summary>
    /// <param name="entity">The entity to apply impulse to.</param>
    /// <param name="impulse">The impulse vector to apply.</param>
    public static void ApplyImpulse(Entity entity, Vector2 impulse)
        => PhysicsAPI.ApplyImpulse(entity.EntityId, impulse.X, impulse.Y);

    /// <summary>
    /// Get the velocity of an entity's rigidbody.
    /// </summary>
    /// <param name="entity">The entity to query.</param>
    /// <returns>The current velocity vector.</returns>
    public static Vector2 GetVelocity(Entity entity)
    {
        PhysicsAPI.GetVelocity(entity.EntityId, out float x, out float y);
        return new Vector2(x, y);
    }

    /// <summary>
    /// Set the velocity of an entity's rigidbody.
    /// </summary>
    /// <param name="entity">The entity to modify.</param>
    /// <param name="velocity">The new velocity vector.</param>
    public static void SetVelocity(Entity entity, Vector2 velocity)
        => PhysicsAPI.SetVelocity(entity.EntityId, velocity.X, velocity.Y);
}
