# GrapeEngine Scripting System Guide

**Version:** 1.0  
**Last Updated:** December 9, 2025

---

## Changelogs

None so far.

---

## Table of Contents

1. [Core Concepts](#core-concepts)
2. [Getting Started](#getting-started)
3. [The ECS Architecture](#the-ecs-architecture)
4. [Systems](#systems)
5. [Entities and Components](#entities-and-components)
6. [Querying Entities](#querying-entities)
7. [Services](#services)
8. [Math Types](#math-types)
9. [Common Patterns](#common-patterns)
10. [Examples](#examples)
11. [Best Practices](#best-practices)

---

## Core Concepts

### The Entity Component System (ECS)

The GrapeEngine uses an **Entity Component System** architecture. This is a pattern where:

- **Entities** are containers that hold data (components)
- **Components** are plain data structures (structs) that represent properties
- **Systems** are logic classes that operate on entities with specific components

**Example:** A player character might be:
- **Entity:** A unique object in your game
- **Components:** LocalTransform (position/rotation), Rigidbody2D (physics), SpriteRenderer2D (graphics)
- **Systems:** A "PlayerMovementSystem" that reads input and updates the transform

### The World

The **World** is your ECS container. It manages all entities and provides the interface for creating, destroying, and querying them. Most of your code will interact with the world.

---

## Getting Started

### Your First System

A system is a C# class that implements `ISystem`. Here's the minimal example:

```csharp
using GrapeEngine.Scripting;

public class MyFirstSystem : ISystem
{
    public void OnCreate(World world)
    {
        Logging.Log("System created!", LogLevel.Info);
    }

    public void OnUpdate(World world, float deltaTime)
    {
        // Your game logic goes here
        Logging.Log($"Frame time: {deltaTime}s", LogLevel.Debug);
    }

    public void OnDestroy(World world)
    {
        Logging.Log("System destroyed!", LogLevel.Info);
    }
}
```

**Key Points:**
- `OnCreate()` - Called once when the system starts. Initialize here.
- `OnUpdate(world, deltaTime)` - Called every frame. Implement your logic here.
- `OnDestroy()` - Called when the system shuts down. Clean up resources here.

---

## The ECS Architecture

### Understanding ECS

The ECS pattern separates **data** from **logic**:

```
┌─────────────────────────────────────────┐
│            World (Container)            │
├─────────────────────────────────────────┤
│ Entity 1                    Entity 2     │
│ ├─ LocalTransform          ├─ LocalTr.. │
│ ├─ Rigidbody2D             ├─ Rigid...  │
│ └─ SpriteRenderer2D        └─ Sprite... │
├─────────────────────────────────────────┤
│ Systems (Logic)                         │
│ ├─ MovementSystem                       │
│ ├─ PhysicsSystem                        │
│ └─ RenderSystem                         │
└─────────────────────────────────────────┘
```

**Benefits:**
- ✅ High performance - data is organized for cache efficiency
- ✅ Data-driven - easily create variations by combining components
- ✅ Reusable - systems work with any entity that has the right components
- ✅ Testable - systems have no hidden dependencies

### Component Registration

Components must be **unmanaged structs** (no reference types) with sequential memory layout:

```csharp
[StructLayout(LayoutKind.Sequential)]
public struct MyComponent
{
    public float Health;
    public int Level;
    public Vector3 Position;
}
```

The system automatically registers components when you first access them. No manual setup required!

---

## Systems

### Lifecycle Methods

Every system has three key moments:

| Method | When | Purpose |
|--------|------|---------|
| `OnCreate(world)` | System starts | Setup, initialization, cache references |
| `OnUpdate(world, deltaTime)` | Every frame | Main game logic, state changes |
| `OnDestroy(world)` | System shuts down | Cleanup, resource release |

### Example: A Complete System

```csharp
public class HealthSystem : ISystem
{
    private World _world;

    public void OnCreate(World world)
    {
        _world = world;
        Logging.Log("Health system initialized", LogLevel.Info);
    }

    public void OnUpdate(World world, float deltaTime)
    {
        // Iterate all entities with Health and other components
        foreach (var result in world.Query<Health>())
        {
            ref var health = ref result.Component;
            
            // Handle regeneration
            health.CurrentHealth = Math.Min(
                health.CurrentHealth + health.RegenPerSecond * deltaTime,
                health.MaxHealth
            );

            // Check for death
            if (health.CurrentHealth <= 0)
            {
                // Handle death logic here
            }
        }
    }

    public void OnDestroy(World world)
    {
        Logging.Log("Health system shutdown", LogLevel.Info);
    }
}
```

---

## Entities and Components

### Creating Entities

```csharp
// Create an empty entity
Entity player = world.CreateEntity();

// Add components to it
player.AddComponent(new LocalTransform
{
    Position = new Vector3(0, 0, 0),
    Rotation = Quaternion.Identity,
    Scale = Vector3.One
});

player.AddComponent(new SpriteRenderer2D
{
    Color = new Color(1, 1, 1, 1),
    TextureId = 0
});
```

### Component Operations

```csharp
// Check if entity has a component
if (entity.HasComponent<LocalTransform>())
{
    // Get a reference to modify it
    ref var transform = ref entity.GetComponent<LocalTransform>();
    transform.Position += new Vector3(1, 0, 0);
}

// Try to get a component safely
if (entity.TryGetComponent<Rigidbody2D>(out var rigidbody))
{
    // Use rigidbody
}

// Add a component
entity.AddComponent(new LinearVelocity2D(new Vector2(5, 0)));

// Remove a component
entity.RemoveComponent<LinearVelocity2D>();

// Check if entity is alive
if (entity.IsAlive)
{
    // Do something
}
```

### Entity Lifecycle

```csharp
// Create entity
Entity enemy = world.CreateEntity();

// ... add components and use it ...

// Destroy entity (removes all components)
enemy.Destroy();

// Check validity
if (!enemy.IsAlive)
{
    // Entity is gone
}
```

---

## Querying Entities

### Basic Iteration

The **Query** system efficiently finds entities with specific components:

```csharp
// Find all entities with LocalTransform
foreach (var result in world.Query<LocalTransform>())
{
    ref var transform = ref result.Component;
    transform.Position += new Vector3(0.1f, 0, 0);
}
```

### Multiple Component Queries

```csharp
// Find entities with Transform AND Rigidbody
foreach (var result in world.Query<LocalTransform, Rigidbody2D>())
{
    ref var transform = ref result.Component1;
    ref var rigidbody = ref result.Component2;
    
    // Both components are available
}

// Query up to 8 components: Query<T1, T2, T3, T4, T5, T6, T7, T8>
```

### Query Filtering

```csharp
// Exclude entities with a component
var query = world.Query<LocalTransform>()
    .Without<Frozen>();

foreach (var result in query)
{
    // Only active, non-frozen entities
}

// Require additional components
var filtered = world.Query<LocalTransform>()
    .WithAll<Rigidbody2D>()
    .Without<Disabled>();

foreach (var result in filtered)
{
    // Entities with Transform and Rigidbody, but not Disabled
}

// Check for optional components
var optional = world.Query<LocalTransform>()
    .Optional<LinearVelocity2D>();
```

### Convenience Methods

```csharp
// Count matching entities
int activeCount = world.Query<LocalTransform>().Count();

// Check if any entity matches
if (world.Query<LocalTransform>().Any())
{
    // At least one entity with LocalTransform exists
}
```

### Query Performance

Queries are optimized for iteration:
- ✅ Construction: O(1) - instant
- ✅ Per-entity: O(1) amortized - constant time
- ✅ No allocations during iteration

---

## Services

Services are static APIs that provide access to engine features. They're always available through global usings.

### Time Service

```csharp
// Read-only properties
float deltaTime = Time.DeltaTime;           // Frame time (scaled)
float unscaledDelta = Time.UnscaledDeltaTime;
float fixedDelta = Time.FixedDeltaTime;
double elapsed = Time.ElapsedTime;          // Total elapsed time
int frame = Time.FrameCount;

// Read/Write
Time.TimeScale = 0.5f;                      // Slow motion
Time.MaximumDeltaTime = 0.1f;               // Cap frame time
```

### Input Service

```csharp
// Keyboard
if (Input.IsKeyPressed(KeyCode.W))
{
    // Key was just pressed this frame
}

if (Input.IsKeyDown(KeyCode.Space))
{
    // Key is held down
}

// Mouse
double mouseX = Input.MouseX;
double mouseY = Input.MouseY;

if (Input.IsMousePressed(0)) // 0 = left button
{
    // Handle click
}

// Scrolling
double scrollY = Input.ScrollY;
```

**Key Codes:**
```csharp
KeyCode.A, KeyCode.B, ... KeyCode.Z
KeyCode.Space, KeyCode.Enter, KeyCode.Escape
KeyCode.Left, KeyCode.Right, KeyCode.Up, KeyCode.Down
KeyCode.Shift, KeyCode.Control, KeyCode.Alt
```

### Logging Service

```csharp
Logging.Log("Game started", LogLevel.Info);
Logging.Log("Debug info", LogLevel.Debug);
Logging.Log("Watch out!", LogLevel.Warning);
Logging.Log("Something broke", LogLevel.Error);
```

### Physics Service

```csharp
// Gravity
Physics.SetGravity(world, new Vector2(0, -9.81f));
Vector2 gravity = Physics.GetGravity(world);

// Control physics
Physics.SetEnabled(world, true);
bool isEnabled = Physics.IsEnabled(world);

// Apply forces
Physics.ApplyForce(world, entity, new Vector2(10, 0));
Physics.ApplyImpulse(world, entity, new Vector2(0, 5));

// Query physics
Vector2 velocity = Physics.GetVelocity(world, entity);
Vector2 angularVelocity = Physics.GetAngularVelocity(world, entity);
```

### Application Service

```csharp
// Application info
string appName = Application.Name;
float fixedTimeStep = Application.FixedTimeStep;
bool vSyncEnabled = Application.IsVSyncEnabled;

// Control application
Application.Quit();
```

---

## Math Types

### Vector2

```csharp
var pos = new Vector2(3, 4);

// Properties
float length = pos.Magnitude;
Vector2 normalized = pos.Normalized;

// Operators
Vector2 v1 = new Vector2(1, 2);
Vector2 v2 = new Vector2(3, 4);
Vector2 sum = v1 + v2;
Vector2 diff = v1 - v2;
Vector2 scaled = v1 * 2.5f;

// Predefined vectors
Vector2.Zero      // (0, 0)
Vector2.One       // (1, 1)
Vector2.Up        // (0, 1)
Vector2.Down      // (0, -1)
Vector2.Left      // (-1, 0)
Vector2.Right     // (1, 0)
```

### Vector3

```csharp
var pos = new Vector3(1, 2, 3);

// Properties
float length = pos.Magnitude;
Vector3 normalized = pos.Normalized;

// Operators
Vector3 v1 = new Vector3(1, 2, 3);
Vector3 v2 = new Vector3(4, 5, 6);
Vector3 sum = v1 + v2;
Vector3 scaled = v1 * 1.5f;

// Predefined vectors
Vector3.Zero      // (0, 0, 0)
Vector3.One       // (1, 1, 1)
Vector3.Up        // (0, 1, 0)
Vector3.Down      // (0, -1, 0)
Vector3.Left      // (-1, 0, 0)
Vector3.Right     // (1, 0, 0)
Vector3.Forward   // (0, 0, 1)
Vector3.Back      // (0, 0, -1)
```

### Quaternion

Quaternions represent rotations (used in LocalTransform):

```csharp
// Identity (no rotation)
var quat = Quaternion.Identity;

// Create from axis and angle
var rotation = Quaternion.AxisAngle(Vector3.Up, 45f * MathF.PI / 180f);

// Use in transforms
var transform = new LocalTransform
{
    Rotation = Quaternion.Identity
};
```

---

## Common Patterns

### Pattern 1: Player Movement System

```csharp
public class PlayerMovementSystem : ISystem
{
    private const float MoveSpeed = 5f;

    public void OnUpdate(World world, float deltaTime)
    {
        foreach (var entity in world.Query<LocalTransform, PlayerTag>())
        {
            ref var transform = ref entity.Component;

            // Read input
            Vector3 movement = Vector3.Zero;
            if (Input.IsKeyDown(KeyCode.W))
                movement += Vector3.Forward;
            if (Input.IsKeyDown(KeyCode.S))
                movement += Vector3.Back;
            if (Input.IsKeyDown(KeyCode.A))
                movement += Vector3.Left;
            if (Input.IsKeyDown(KeyCode.D))
                movement += Vector3.Right;

            // Normalize to prevent faster diagonal movement
            if (movement.Magnitude > 0)
                movement = movement.Normalized;

            // Apply movement
            transform.Position += movement * MoveSpeed * deltaTime;
        }
    }
}
```

### Pattern 2: Lifetime System

```csharp
[StructLayout(LayoutKind.Sequential)]
public struct Lifetime
{
    public float RemainingSeconds;
}

public class LifetimeSystem : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        foreach (var result in world.Query<Lifetime>())
        {
            ref var lifetime = ref result.Component;
            lifetime.RemainingSeconds -= deltaTime;

            if (lifetime.RemainingSeconds <= 0)
            {
                result.Entity.Destroy();
            }
        }
    }
}
```

### Pattern 3: Physics-Based Movement

```csharp
public class PhysicsMovementSystem : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        foreach (var result in world.Query<Rigidbody2D, LinearVelocity2D>())
        {
            ref var rb = ref result.Component1;
            ref var vel = ref result.Component2;

            // Apply gravity
            if (rb.UseGravity)
            {
                Vector2 gravity = Physics.GetGravity(world);
                Physics.ApplyForce(world, result.Entity, gravity * rb.Mass);
            }

            // Apply velocity
            Physics.ApplyForce(world, result.Entity, vel.Value * rb.Mass);
        }
    }
}
```

### Pattern 4: Collision Handling

```csharp
public class CollisionSystem : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        // Check all entities that collided this frame
        foreach (var entity in world.Query<LocalTransform>()
            .Optional<CollisionEvent>())
        {
            if (entity.Entity.HasComponent<CollisionEvent>())
            {
                // Handle collision
                var collision = entity.Entity.GetComponent<CollisionEvent>();
                Logging.Log($"Entity {entity.Entity.Id} collided!", LogLevel.Debug);
            }
        }
    }
}
```

---

## Examples

### Example 1: Simple Sprite Movement

```csharp
// Component for marking enemies
public struct EnemyTag { }

public class EnemyMovementSystem : ISystem
{
    private float _time = 0;

    public void OnUpdate(World world, float deltaTime)
    {
        _time += deltaTime;

        foreach (var result in world.Query<LocalTransform, EnemyTag>())
        {
            ref var transform = ref result.Component;
            
            // Simple oscillating movement
            transform.Position = new Vector3(
                MathF.Sin(_time) * 5,
                transform.Position.Y,
                transform.Position.Z
            );
        }
    }
}
```

### Example 2: Score Tracking System

```csharp
[StructLayout(LayoutKind.Sequential)]
public struct Score
{
    public int Points;
    public int Level;
}

public class ScoreSystem : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        // Find player and update score
        foreach (var result in world.Query<Score, PlayerTag>())
        {
            ref var score = ref result.Component;
            
            // Increase score over time (for testing)
            score.Points += 1;
            
            // Check for level up
            if (score.Points > score.Level * 1000)
            {
                score.Level++;
                Logging.Log($"Level up! Now level {score.Level}", LogLevel.Info);
            }
        }
    }
}
```

### Example 3: Animation System

```csharp
public class AnimationSystem : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        foreach (var result in world.Query<
            SpriteSheetAnimation2D, 
            AnimationState2D, 
            SpriteRenderer2D>())
        {
            var anim = result.Component1;
            ref var state = ref result.Component2;
            ref var sprite = ref result.Component3;

            if (!anim.Playing)
                return;

            // Update animation
            state.TimeAccumulator += deltaTime;
            float frameDuration = 1f / anim.FramesPerSecond;

            if (state.TimeAccumulator >= frameDuration)
            {
                state.TimeAccumulator -= frameDuration;
                state.CurrentFrame++;

                if (state.CurrentFrame >= anim.FrameCount)
                {
                    if (anim.Loop)
                        state.CurrentFrame = 0;
                    else
                        state.Finished = true;
                }
            }
        }
    }
}
```

---

## Best Practices

### ✅ Do

- **Use queries for iteration** - They're optimized and clean
- **Keep components data-only** - No logic, no references
- **Separate concerns into systems** - One system per responsibility
- **Cache world reference** - Store it in OnCreate, reuse it
- **Use meaningful component names** - Make intent clear
- **Handle edge cases** - Check IsAlive, use TryGetComponent
- **Log important events** - Makes debugging easier

### ❌ Don't

- **Store managed objects in components** - Breaks C# ↔ C++ marshaling
- **Modify entities while iterating** - Can cause corruption
- **Create systems with side effects** - Keep them pure
- **Ignore DeltaTime** - Always scale movement and time-based changes
- **Iterate the same query multiple times per frame** - Cache the result
- **Use reference types in components** - Only unmanaged structs
- **Forget to handle destroyed entities** - Always check IsAlive

### Component Guidelines

```csharp
// ✅ GOOD - Data only, unmanaged
[StructLayout(LayoutKind.Sequential)]
public struct PlayerData
{
    public float Health;
    public int Score;
    public Vector3 SpawnPoint;
}

// ❌ BAD - Contains reference types
public struct BadComponent
{
    public string Name;           // Reference type - NO!
    public List<int> Items;       // Reference type - NO!
    public float Health;
}

// ❌ BAD - Contains methods/logic
public struct LogicComponent
{
    public float Health;
    
    public void TakeDamage(float amount)  // NO! Components = data only
    {
        Health -= amount;
    }
}
```

### Query Best Practices

```csharp
// ✅ GOOD - Cache complex queries
public class MySystem : ISystem
{
    private World _world;

    public void OnCreate(World world)
    {
        _world = world;
    }

    public void OnUpdate(World world, float deltaTime)
    {
        // Query once per frame, iterate multiple times if needed
        var movingEntities = _world.Query<LocalTransform>()
            .Without<Frozen>();
        
        foreach (var result in movingEntities)
        {
            // Use result
        }
    }
}

// ❌ AVOID - Creating query in tight loop
public void BadApproach(World world)
{
    for (int i = 0; i < 100; i++)
    {
        foreach (var result in world.Query<LocalTransform>()) // Inefficient!
        {
            // ...
        }
    }
}
```

---

## Architecture Overview

```
┌─────────────────────────────────────────────┐
│         GrapeEngine Scripting API           │
├─────────────────────────────────────────────┤
│ Your Systems (C#)                           │
│  ├─ MovementSystem                          │
│  ├─ PhysicsSystem                           │
│  └─ RenderSystem                            │
├─────────────────────────────────────────────┤
│ Core ECS Layer                              │
│  ├─ World      (Entity container)           │
│  ├─ Entity     (Component container)        │
│  └─ Query      (Entity iteration)           │
├─────────────────────────────────────────────┤
│ Services (Static APIs)                      │
│  ├─ Input, Physics, Time, Logging...        │
├─────────────────────────────────────────────┤
│ Math Types                                  │
│  ├─ Vector2, Vector3, Quaternion            │
├─────────────────────────────────────────────┤
│ Unsafe Interop (P/Invoke to C++)            │
│  ├─ WorldAPI, InputAPI, PhysicsAPI...       │
├─────────────────────────────────────────────┤
│ GrapeEngine C++ Core (Performance)          │
└─────────────────────────────────────────────┘
```

---

## Hot Reload Support

The scripting system supports **hot reload** - recompiling and reloading scripts without restarting the engine. During reload:

- ✅ Systems are recompiled
- ✅ State is preserved for compatible systems
- ✅ Entities and components persist
- ✅ You can iterate on gameplay quickly

Just save your changes and the engine automatically reloads!

---

## Troubleshooting

### "Component not registered"
Components are auto-registered on first use. If you get this error, ensure your component is a proper unmanaged struct.

### "Entity is not alive"
Check if the entity was destroyed. Use `entity.IsAlive` before accessing it.

### "Query returns no results"
Verify entities have all the components in your query. Use `HasComponent<T>()` to debug.

### Performance issues
- ✅ Reduce query iterations by filtering better
- ✅ Avoid allocations in OnUpdate
- ✅ Cache query results outside tight loops
- ✅ Use `Without<>()` to exclude expensive components

---

## Quick Reference

| Task | Code |
|------|------|
| Create entity | `Entity e = world.CreateEntity()` |
| Add component | `e.AddComponent(new MyComponent { ... })` |
| Get component | `ref var c = ref e.GetComponent<MyComponent>()` |
| Check component | `if (e.HasComponent<MyComponent>())` |
| Remove component | `e.RemoveComponent<MyComponent>()` |
| Destroy entity | `e.Destroy()` |
| Query entities | `foreach (var r in world.Query<ComponentType>())` |
| Filter query | `world.Query<T>().Without<U>()` |
| Get time | `float dt = Time.DeltaTime` |
| Get input | `Input.IsKeyDown(KeyCode.W)` |
| Log message | `Logging.Log("msg", LogLevel.Info)` |
| Apply force | `Physics.ApplyForce(world, entity, force)` |
