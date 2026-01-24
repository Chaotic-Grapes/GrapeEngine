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

using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Internal.Profiling;
using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Physics utilities for working with rigidbodies and forces.
/// </summary>
public static class Physics
{
    /// <summary>
    /// Set the global gravity for the physics world.
    /// </summary>
    public static void SetGravity(World world, Vector2 gravity)
    {
        using (PInvokeTimer.Start("PhysicsAPI.SetGravity"))
        {
            unsafe
            {
                PhysicsAPI.SetGravity(world.NativePtr, gravity.X, gravity.Y);
            }
        }
    }

    /// <summary>
    /// Get the global gravity for the physics world.
    /// </summary>
    public static Vector2 GetGravity(World world)
    {
        using (PInvokeTimer.Start("PhysicsAPI.GetGravity"))
        {
            unsafe
            {
                float x, y;
                PhysicsAPI.GetGravity(world.NativePtr, &x, &y);
                return new Vector2(x, y);
            }
        }
    }

    /// <summary>
    /// Enable or disable the physics system.
    /// </summary>
    public static void SetEnabled(World world, bool enabled)
    {
        using (PInvokeTimer.Start("PhysicsAPI.SetEnabled"))
        {
            unsafe
            {
                PhysicsAPI.SetEnabled(world.NativePtr, enabled);
            }
        }
    }

    /// <summary>
    /// Check if the physics system is enabled.
    /// </summary>
    public static bool IsEnabled(World world)
    {
        using (PInvokeTimer.Start("PhysicsAPI.IsEnabled"))
        {
            unsafe
            {
                return PhysicsAPI.IsEnabled(world.NativePtr);
            }
        }
    }

    /// <summary>
    /// Apply a force to an entity with a Rigidbody2D.
    /// </summary>
    public static void ApplyForce(World world, Entity entity, Vector2 force)
    {
        using (PInvokeTimer.Start("PhysicsAPI.ApplyForce"))
        {
            unsafe
            {
                PhysicsAPI.ApplyForce(world.NativePtr, entity.Id, force.X, force.Y);
            }
        }
    }

    /// <summary>
    /// Apply an impulse to an entity with a Rigidbody2D.
    /// </summary>
    public static void ApplyImpulse(World world, Entity entity, Vector2 impulse)
    {
        using (PInvokeTimer.Start("PhysicsAPI.ApplyImpulse"))
        {
            unsafe
            {
                PhysicsAPI.ApplyImpulse(world.NativePtr, entity.Id, impulse.X, impulse.Y);
            }
        }
    }

    /// <summary>
    /// Get the velocity of an entity with a Rigidbody2D.
    /// </summary>
    public static Vector2 GetVelocity(World world, Entity entity)
    {
        using (PInvokeTimer.Start("PhysicsAPI.GetVelocity"))
        {
            unsafe
            {
                float x, y;
                PhysicsAPI.GetVelocity(world.NativePtr, entity.Id, &x, &y);
                return new Vector2(x, y);
            }
        }
    }

    /// <summary>
    /// Set the velocity of an entity with a Rigidbody2D.
    /// </summary>
    public static void SetVelocity(World world, Entity entity, Vector2 velocity)
    {
        using (PInvokeTimer.Start("PhysicsAPI.SetVelocity"))
        {
            unsafe
            {
                PhysicsAPI.SetVelocity(world.NativePtr, entity.Id, velocity.X, velocity.Y);
            }
        }
    }
}

