/* Start Header *****************************************************************/
/*!
\file   TestMovementSystem.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Example C# system demonstrating World/Entity interop for scripting.
This system moves entities with a Velocity component.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine;
using System.Numerics;

namespace GrapeEngine.Scripting;

/// <summary>
/// Example system that demonstrates World/Entity access from C#.
/// This will be discovered by ScriptManager and registered as an ECS system.
/// </summary>
public class TestMovementSystem : ISystem
{
    public void OnCreate(World world)
    {
        Console.WriteLine("[TestMovementSystem] OnCreate called!");
        Console.WriteLine("[TestMovementSystem] World interop bridge is working!");
    }

    public void OnUpdate(World world, float deltaTime)
    {
        // Query API example: Iterate over all entities with Position and Velocity
        foreach (var (entity, pos, vel) in world.Query<Position, Velocity>())
        {
            // Modify component data directly (zero-copy access to native memory)
            pos.Value += vel.Value * deltaTime;
        }
        
        // Note: Components are accessed by reference, so modifications persist
        // across frames. No need to "set" components back to the entity.
    }

    public void OnDestroy(World world)
    {
        Console.WriteLine("[TestMovementSystem] OnDestroy called!");
    }
}

// Example component structs (these would normally be defined alongside C++ components)
public struct Position
{
    public Vector3 Value;
}

public struct Velocity
{
    public Vector3 Value;
}
