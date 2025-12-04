/* Query API Usage Guide - C# Scripting for GrapeEngine ECS
 * ============================================================
 * 
 * The Query API enables efficient iteration over entities matching specific
 * component signatures. This document explains usage patterns and implementation.
 */

// ==============================================================================
// BASIC USAGE
// ==============================================================================

public class ExampleSystem : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        // Single component query
        foreach (var (entity, transform) in world.Query<Transform>())
        {
            // Read/write transform component
            transform.Position += Vector3.UnitY * deltaTime;
        }

        // Two component query
        foreach (var (entity, pos, vel) in world.Query<Position, Velocity>())
        {
            // Zero-copy access - modifications persist automatically
            pos.Value += vel.Value * deltaTime;
        }

        // Three component query
        foreach (var (entity, pos, rot, scale) in world.Query<Position, Rotation, Scale>())
        {
            // Access multiple components per entity
            var transform = Matrix4x4.CreateScale(scale.Value) * 
                           Matrix4x4.CreateFromQuaternion(rot.Value) * 
                           Matrix4x4.CreateTranslation(pos.Value);
        }
    }
}

// ==============================================================================
// COMPONENT DEFINITION
// ==============================================================================

// Components MUST be unmanaged structs (no references, no managed memory)
// They should match C++ component layouts for interop

public struct Transform
{
    public Vector3 Position;
    public Quaternion Rotation;
    public Vector3 Scale;
}

public struct RigidBody
{
    public Vector3 Velocity;
    public Vector3 AngularVelocity;
    public float Mass;
}

public struct Health
{
    public float Current;
    public float Max;
}

// ==============================================================================
// ADVANCED PATTERNS
// ==============================================================================

public class PhysicsSystem : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        // Apply gravity to all dynamic rigid bodies
        foreach (var (entity, rb, transform) in world.Query<RigidBody, Transform>())
        {
            rb.Velocity += Vector3.UnitY * -9.8f * deltaTime;
            transform.Position += rb.Velocity * deltaTime;
        }
    }
}

public class HealthDisplaySystem : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        // Process entities with Health component only
        foreach (var (entity, health) in world.Query<Health>())
        {
            if (health.Current <= 0)
            {
                // Entity is dead - could destroy or disable
                entity.Destroy();
            }
        }
    }
}

// ==============================================================================
// IMPLEMENTATION DETAILS
// ==============================================================================

/*
 * ZERO-COPY ARCHITECTURE
 * ----------------------
 * Component references point directly to C++ ECS memory:
 * 
 *   C++ Memory                    C# Reference
 *   ┌──────────────┐             ┌──────────────┐
 *   │ Position[0]  │◄────────────│ ref Position │
 *   │ Position[1]  │             └──────────────┘
 *   │ Position[2]  │
 *   └──────────────┘
 * 
 * Modifications are directly written to native memory - no marshalling overhead.
 * 
 * 
 * TYPE IDENTIFICATION
 * -------------------
 * Components are identified across C++/C# boundary using FNV-1a hash:
 * 
 *   C#: ComponentTypeHelper.GetTypeHash<Position>()
 *       → 0xDEADBEEF (hash of "Position")
 * 
 *   C++: ComponentRegistry::GetComponentIdFromHash(0xDEADBEEF)
 *       → ComponentId(3)
 * 
 * Hash must be registered on both sides before use!
 * 
 * 
 * QUERY EXECUTION FLOW
 * ---------------------
 * 
 * 1. C#: world.Query<Position, Velocity>()
 *    → Creates Query<Position, Velocity> wrapper
 *    → Computes type hashes: [hash(Position), hash(Velocity)]
 * 
 * 2. C#: foreach begins → GetEnumerator()
 *    → Creates QueryEnumerator<Position, Velocity>
 *    → Calls QueryInteropAPI.CreateQuery()
 * 
 * 3. C++: WorldInterop_CreateQuery()
 *    → Converts hashes to ComponentIds
 *    → Creates signature: {ComponentId(1), ComponentId(2)}
 *    → Finds matching archetypes via World::GetMatchingArchetypes()
 *    → Initializes QueryIterator
 * 
 * 4. C#: foreach iteration → MoveNext()
 *    → Calls QueryInteropAPI.QueryNext()
 * 
 * 5. C++: WorldInterop_QueryNext()
 *    → Advances to next entity in current chunk
 *    → Or advances to next chunk in current archetype
 *    → Or advances to next archetype
 *    → Returns entity ID
 * 
 * 6. C#: foreach body → Current property
 *    → Calls QueryInteropAPI.QueryGetComponent(index: 0) → Position*
 *    → Calls QueryInteropAPI.QueryGetComponent(index: 1) → Velocity*
 *    → Creates QueryResult<Position, Velocity> with refs
 *    → Deconstructs into (entity, pos, vel)
 * 
 * 7. Repeat steps 4-6 until no more entities
 * 
 * 
 * PERFORMANCE CHARACTERISTICS
 * ---------------------------
 * - Iteration is cache-friendly (components stored contiguously per archetype)
 * - Zero marshalling overhead (direct pointer access)
 * - Query setup cost: O(archetypes) for signature matching
 * - Iteration cost: O(entities) with good cache locality
 * - No allocations during iteration (struct enumerators)
 */

// ==============================================================================
// EXTENSION POINTS
// ==============================================================================

/*
 * TO ADD MORE COMPONENT SLOTS:
 * 
 * 1. Update QueryIterator.ComponentTypeIds array size in WorldInterop.h
 *    fixed uint ComponentTypeIds[8];  // Current limit: 8 components
 * 
 * 2. Add Query<T1, T2, T3, T4> in Query.cs
 * 3. Add QueryResult<T1, T2, T3, T4> in QueryResult.cs
 * 4. Add QueryEnumerator<T1, T2, T3, T4> in QueryEnumerator.cs
 * 
 * 
 * TO ADD QUERY FILTERS (e.g., Without<T>):
 * 
 * 1. Extend QueryIterator to support exclusion signatures
 * 2. Update WorldInterop_CreateQuery to accept excludeHashes
 * 3. Modify GetMatchingArchetypes to filter excluded components
 * 4. Create Query<T1, T2>.Without<T3>() builder pattern
 */
