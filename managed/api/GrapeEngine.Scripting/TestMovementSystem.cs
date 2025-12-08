/* Start Header *****************************************************************/
/*!
\file   TestMovementSystem.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Example C# system demonstrating the new record struct component pattern and 
ECS World/Entity interop for gameplay scripting. This system updates entity 
positions based on their velocities.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Query;
using GrapeEngine.Scripting.Systems;
using System.Numerics;

namespace GrapeEngine.Scripting;

/// <summary>
/// Example system demonstrating the record struct component pattern and
/// how to write efficient ECS systems in C#.
/// 
/// This system:
/// 1. Queries entities with Transform and Velocity components
/// 2. Updates positions based on velocity and deltaTime
/// 3. Demonstrates immutable component updates using `with` expressions
/// </summary>
public class TestMovementSystem : ISystem
{
    /// <summary>
    /// Cached query for faster iteration (optional, but recommended for frequently-used queries).
    /// </summary>
    private Query<Transform, Velocity> _query;

    public void OnCreate(World world)
    {
        Console.WriteLine("[TestMovementSystem] System created");
        
        // Cache the query for better performance in OnUpdate
        _query = world.Query<Transform, Velocity>();
    }

    public void OnUpdate(World world, float deltaTime)
    {
        // Use cached query to iterate over all entities with Transform and Velocity
        foreach (var (entity, transform, velocity) in _query)
        {
            // Update position using immutable `with` expression
            // This is the new preferred pattern for record struct components
            var newTransform = transform with 
            { 
                Position = transform.Position + velocity.Value * deltaTime 
            };
            
            // Write updated component back to entity
            entity.SetComponent(newTransform);
        }
    }

    public void OnDestroy(World world)
    {
        Console.WriteLine("[TestMovementSystem] System destroyed");
    }
}
