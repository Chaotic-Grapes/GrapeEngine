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
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Profiling;
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
    /// <sing (PInvokeTimer.Start("PhysicsAPI.SetEnabled"))
        {
            unsafe
            {
                PhysicsAPI.SetEnabled(world.NativePtr, enabled);
            }
        unsafe
        {
            PhysicsAPI.SetEnabled(world.NativePtr, enabled);
        }
    }

    /// <summary>
    /// Check if the physics system is enabled.
    /// <sing (PInvokeTimer.Start("PhysicsAPI.IsEnabled"))
        {
            unsafe
            {
                return PhysicsAPI.IsEnabled(world.NativePtr);
            }
        unsafe
        {
            return PhysicsAPI.IsEnabled(world.NativePtr);
        }
    }
sing (PInvokeTimer.Start("PhysicsAPI.ApplyForce"))
        {
            unsafe
            {
                PhysicsAPI.ApplyForce(world.NativePtr, entity.Id, force.X, force.Y);
            }
    /// </summary>
    public static void ApplyForce(World world, Entity entity, Vector2 force)
    {
        unsafe
        {
            PhysicsAPI.ApplyForce(world.NativePtr, entity.Id, force.X, force.Y);
        }
    }
sing (PInvokeTimer.Start("PhysicsAPI.ApplyImpulse"))
        {
            unsafe
            {
                PhysicsAPI.ApplyImpulse(world.NativePtr, entity.Id, impulse.X, impulse.Y);
            }
    /// </summary>
    public static void ApplyImpulse(World world, Entity entity, Vector2 impulse)
    {
        unsafe
        {
            PhysicsAPI.ApplyImpulse(world.NativePtr, entity.Id, impulse.X, impulse.Y);
        }
    }
sing (PInvokeTimer.Start("PhysicsAPI.GetVelocity"))
        {
            unsafe
            {
                float x, y;
                PhysicsAPI.GetVelocity(world.NativePtr, entity.Id, &x, &y);
                return new Vector2(x, y);
            }(World world, Entity entity)
    {
        unsafe
        {
            float x, y;
            PhysicsAPI.GetVelocity(world.NativePtr, entity.Id, &x, &y);
         sing (PInvokeTimer.Start("PhysicsAPI.SetVelocity"))
        {
            unsafe
            {
                PhysicsAPI.SetVelocity(world.NativePtr, entity.Id, velocity.X, velocity.Y);
            }

    /// <summary>
    /// Set the velocity of an entity with a Rigidbody2D.
    /// </summary>
    public static void SetVelocity(World world, Entity entity, Vector2 velocity)
    {
        unsafe
        {
            PhysicsAPI.SetVelocity(world.NativePtr, entity.Id, velocity.X, velocity.Y);
        }
    }
}
