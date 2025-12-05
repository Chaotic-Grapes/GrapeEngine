/* Start Header *****************************************************************/
/*!
\file   Physics.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
High-level physics API for C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;
using System.Numerics;

namespace GrapeEngine.Scripting;

/// <summary>
/// Physics utilities for working with rigidbodies and forces.
/// </summary>
public static class Physics
{
    /// <summary>
    /// Set the global gravity for the physics world.
    /// </summary>
    public static unsafe void SetGravity(World world, Vector2 gravity)
    {
        WorldInteropAPI.Physics_SetGravity(world.NativePtr, gravity.X, gravity.Y);
    }

    /// <summary>
    /// Get the global gravity for the physics world.
    /// </summary>
    public static unsafe Vector2 GetGravity(World world)
    {
        float x, y;
        WorldInteropAPI.Physics_GetGravity(world.NativePtr, &x, &y);
        return new Vector2(x, y);
    }

    /// <summary>
    /// Enable or disable the physics system.
    /// </summary>
    public static unsafe void SetEnabled(World world, bool enabled)
    {
        WorldInteropAPI.Physics_SetEnabled(world.NativePtr, enabled);
    }

    /// <summary>
    /// Check if the physics system is enabled.
    /// </summary>
    public static unsafe bool IsEnabled(World world)
    {
        return WorldInteropAPI.Physics_IsEnabled(world.NativePtr);
    }

    /// <summary>
    /// Apply a force to an entity with a Rigidbody2D.
    /// </summary>
    public static unsafe void ApplyForce(World world, Entity entity, Vector2 force)
    {
        WorldInteropAPI.Physics_ApplyForce(world.NativePtr, entity.Id, force.X, force.Y);
    }

    /// <summary>
    /// Apply an impulse to an entity with a Rigidbody2D.
    /// </summary>
    public static unsafe void ApplyImpulse(World world, Entity entity, Vector2 impulse)
    {
        WorldInteropAPI.Physics_ApplyImpulse(world.NativePtr, entity.Id, impulse.X, impulse.Y);
    }

    /// <summary>
    /// Get the velocity of an entity with a Rigidbody2D.
    /// </summary>
    public static unsafe Vector2 GetVelocity(World world, Entity entity)
    {
        float x, y;
        WorldInteropAPI.Physics_GetVelocity(world.NativePtr, entity.Id, &x, &y);
        return new Vector2(x, y);
    }

    /// <summary>
    /// Set the velocity of an entity with a Rigidbody2D.
    /// </summary>
    public static unsafe void SetVelocity(World world, Entity entity, Vector2 velocity)
    {
        WorldInteropAPI.Physics_SetVelocity(world.NativePtr, entity.Id, velocity.X, velocity.Y);
    }
}
