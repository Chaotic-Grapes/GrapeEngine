# C# Scripting Quick Reference

## System Template
```csharp
using GrapeEngine;
using System.Numerics;

public class MySystem : ISystem
{
    public void OnCreate(World world)
    {
        // Initialize system, register components, create entities
    }
    
    public void OnUpdate(World world, float deltaTime)
    {
        // Main game logic
    }
    
    public void OnDestroy(World world)
    {
        // Cleanup
    }
}
```

## Component Definition
```csharp
// Must be unmanaged struct (no references, no managed memory)
public struct MyComponent
{
    public float Value;
    public Vector3 Position;
    public int Count;
}
```

## Entity Operations
```csharp
// Create entity
var entity = world.CreateEntity();

// Add component
entity.AddComponent(new Position { Value = Vector3.Zero });

// Get component (ref for modification)
ref var pos = ref entity.GetComponent<Position>();
pos.Value.Y += 10f;

// Check component
if (entity.HasComponent<Velocity>())
{
    // ...
}

// Remove component
entity.RemoveComponent<OldComponent>();

// Destroy entity
entity.Destroy();

// Check if alive
if (entity.IsAlive)
{
    // ...
}
```

## Query API
```csharp
// Single component
foreach (var (entity, transform) in world.Query<Transform>())
{
    transform.Position += Vector3.UnitY * deltaTime;
}

// Two components
foreach (var (entity, pos, vel) in world.Query<Position, Velocity>())
{
    pos.Value += vel.Value * deltaTime;
}

// Three components
foreach (var (entity, pos, rot, scale) in world.Query<Position, Rotation, Scale>())
{
    // Access multiple components
}
```

## Component Registration
```csharp
// Automatic (recommended)
entity.AddComponent<MyComponent>();  // Auto-registers on first use

// Manual (in OnCreate)
ComponentRegistry.Register<MyComponent>();

// Check registration
if (ComponentRegistry.IsRegistered<MyComponent>())
{
    // ...
}
```

## Hot Reload Safe Patterns

### ❌ BAD: State in System
```csharp
public class BadSystem : ISystem
{
    private float _timer = 0f;  // Lost on reload!
    
    public void OnUpdate(World world, float deltaTime)
    {
        _timer += deltaTime;  // Resets to 0 after reload
    }
}
```

### ✅ GOOD: State in ECS
```csharp
public struct TimerState { public float Value; }

public class GoodSystem : ISystem
{
    public void OnCreate(World world)
    {
        var timer = world.CreateEntity();
        timer.AddComponent(new TimerState { Value = 0f });
    }
    
    public void OnUpdate(World world, float deltaTime)
    {
        foreach (var (entity, timer) in world.Query<TimerState>())
        {
            timer.Value += deltaTime;  // Persists across reloads!
        }
    }
}
```

## Common Patterns

### Singleton Entity
```csharp
public struct GameConfig
{
    public float SpawnRate;
    public int MaxEnemies;
}

public void OnCreate(World world)
{
    var config = world.CreateEntity();
    config.AddComponent(new GameConfig 
    { 
        SpawnRate = 2.0f,
        MaxEnemies = 50
    });
}
```

### Timer Pattern
```csharp
public struct Timer
{
    public float Elapsed;
    public float Duration;
    public bool IsComplete => Elapsed >= Duration;
}

foreach (var (entity, timer) in world.Query<Timer>())
{
    timer.Elapsed += deltaTime;
    if (timer.IsComplete)
    {
        // Do something
        entity.RemoveComponent<Timer>();
    }
}
```

### Spawner Pattern
```csharp
public struct Spawner
{
    public float TimeSinceLastSpawn;
    public float SpawnInterval;
    public int MaxSpawns;
    public int SpawnCount;
}

foreach (var (entity, spawner) in world.Query<Spawner>())
{
    spawner.TimeSinceLastSpawn += deltaTime;
    
    if (spawner.TimeSinceLastSpawn >= spawner.SpawnInterval &&
        spawner.SpawnCount < spawner.MaxSpawns)
    {
        // Spawn entity
        var spawned = world.CreateEntity();
        spawned.AddComponent(new Position { Value = Vector3.Zero });
        
        spawner.SpawnCount++;
        spawner.TimeSinceLastSpawn = 0f;
    }
}
```

## Performance Tips

### ✅ Do This
- Store state in components
- Use foreach queries for iteration
- Access components by ref
- Keep component structs small
- Use Vector3/Vector2 for spatial data

### ❌ Don't Do This
- Store references to entities (use IDs)
- Create allocations in OnUpdate
- Use managed types in components
- Change component layouts during hot reload
- Use static fields in systems

## Debugging

### Console Output
```csharp
Console.WriteLine($"Entity {entity.Id}: pos={pos.Value}");
```

### Check Entity State
```csharp
if (!entity.IsAlive)
{
    Console.WriteLine("Entity was destroyed!");
    continue;
}
```

### Validate Components
```csharp
if (!entity.HasComponent<RequiredComponent>())
{
    Console.WriteLine("Missing required component!");
    return;
}
```

## Common Errors

### "Component not registered"
**Solution**: Component auto-registers on first use. If error persists, manually call:
```csharp
ComponentRegistry.Register<MyComponent>();
```

### "Entity does not have component"
**Solution**: Check with `HasComponent<T>()` before calling `GetComponent<T>()`
```csharp
if (entity.HasComponent<Health>())
{
    ref var health = ref entity.GetComponent<Health>();
}
```

### "Memory corruption after hot reload"
**Cause**: Changed component struct layout
**Solution**: Restart application, don't change component definitions during hot reload

### "State lost after hot reload"
**Cause**: State stored in system fields
**Solution**: Store state in ECS components instead

## Documentation
- Query API: `docs/QUERY_API_GUIDE.md`
- Hot Reload: `docs/COMPONENT_REGISTRATION_AND_HOT_RELOAD.md`
- Full Summary: `docs/SCRIPTING_IMPLEMENTATION_SUMMARY.md`
