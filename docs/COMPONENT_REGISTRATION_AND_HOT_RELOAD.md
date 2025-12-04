# Component Registration and Hot Reload Guide

## Component Registration

### Overview
Components must be registered with the ECS before they can be used in queries or entity operations. The scripting system provides automatic registration for C# components.

### Automatic Registration
Components are automatically registered when first used in any of these operations:
- `entity.AddComponent<T>()`
- `entity.GetComponent<T>()`
- `entity.HasComponent<T>()`
- `entity.RemoveComponent<T>()`
- `world.Query<T1, T2>()`

### Manual Registration
You can also manually register components in your system's `OnCreate()` method:

```csharp
public class MySystem : ISystem
{
    public void OnCreate(World world)
    {
        // Manually register custom components
        ComponentRegistry.Register<MyCustomComponent>();
        ComponentRegistry.Register<AnotherComponent>();
        
        // Check if a component is registered
        bool isRegistered = ComponentRegistry.IsRegistered<MyCustomComponent>();
    }
    
    public void OnUpdate(World world, float deltaTime)
    {
        // Components are already registered, safe to use
        foreach (var (entity, component) in world.Query<MyCustomComponent>())
        {
            // Process component data
        }
    }
}
```

### Component Type Hashing
Components are identified across the C++/C# boundary using **FNV-1a hashing**:

```csharp
// C# side computes hash of type name
uint hash = ComponentTypeHelper.GetTypeHash<Position>();
// Hash: 0x???????? (hash of "Position")

// C++ side looks up ComponentId from hash
ComponentId id = ComponentRegistry::GetComponentIdFromHash(hash);
```

**Important**: Component type names must match between C++ and C# for interop to work!

### C++ Component Registration
For components defined in C++, register them with their hash during engine initialization:

```cpp
// In engine initialization code
using ECS::ComponentRegistry;

// Register C++ components with hashes that match C# type names
ComponentRegistry::RegisterWithHash<Position>(0x????????);  // Use same hash as C#
ComponentRegistry::RegisterWithHash<Velocity>(0x????????);
```

---

## Hot Reload System

### Overview
The hot reload system allows C# scripts to be modified, recompiled, and reloaded without restarting the application. This dramatically improves iteration speed during development.

### Architecture

```
┌────────────────┐
│ FileWatcher    │ Monitors .cs files for changes
└───────┬────────┘
        │ File changed event
        ▼
┌────────────────┐
│ Debounce Timer │ Waits 500ms for multiple changes
└───────┬────────┘
        │ Timer elapsed
        ▼
┌────────────────┐
│ Recompile      │ Rebuild changed assembly
└───────┬────────┘
        │ New .dll
        ▼
┌────────────────┐
│ Unload Context │ Destroy old system instances
└───────┬────────┘
        │ AssemblyLoadContext.Unload()
        ▼
┌────────────────┐
│ Load Context   │ Load new assembly version
└───────┬────────┘
        │ New types
        ▼
┌────────────────┐
│ Recreate       │ Instantiate new system instances
└────────────────┘
```

### Enabling Hot Reload

#### From C++
```cpp
// In ScriptManager initialization
scriptManager.EnableHotReload("path/to/scripts");
```

#### From C#
```csharp
// In ScriptHost
ScriptFileWatcher.StartWatching("path/to/scripts", callbackPtr);
```

### Hot Reload Workflow

1. **Edit Scripts**
   - Modify `.cs` files in your scripts directory
   - Save changes

2. **Detection**
   - `FileSystemWatcher` detects file changes
   - Debounce timer waits 500ms for additional changes

3. **Unload**
   - Call `ScriptHost.UnloadAssembly(assemblyPath)`
   - Destroys all system instances from that assembly
   - Calls `AssemblyLoadContext.Unload()`
   - Forces garbage collection to reclaim memory

4. **Reload**
   - Call `ScriptHost.LoadAssembly(assemblyPath)`
   - Creates new `AssemblyLoadContext` (collectible)
   - Loads new assembly version

5. **Rediscover**
   - Call `ScriptHost.DiscoverSystems()`
   - Finds all `ISystem` implementations in new assembly
   - Returns handles to C++

6. **Recreate**
   - C++ calls `ScriptHost.CreateSystemInstance()` for each system
   - New instances replace old ones in system manager

### Hot Reload Limitations

#### ⚠️ State is Lost
System instance state is **not preserved** during reload. Any fields/properties in your system class will be reset to defaults.

**Example - State Loss:**
```csharp
public class EnemySpawner : ISystem
{
    private float _timeSinceLastSpawn = 0f; // ❌ Lost on reload!
    private int _enemiesSpawned = 0;        // ❌ Lost on reload!
    
    public void OnUpdate(World world, float deltaTime)
    {
        _timeSinceLastSpawn += deltaTime;
        if (_timeSinceLastSpawn >= 5.0f)
        {
            // Spawn enemy...
            _enemiesSpawned++;
            _timeSinceLastSpawn = 0f;
        }
    }
}
```

**Solution - Store State in ECS:**
```csharp
public struct SpawnerState
{
    public float TimeSinceLastSpawn;
    public int EnemiesSpawned;
}

public class EnemySpawner : ISystem
{
    public void OnUpdate(World world, float deltaTime)
    {
        // State stored in ECS component - survives reload!
        foreach (var (entity, state) in world.Query<SpawnerState>())
        {
            state.TimeSinceLastSpawn += deltaTime;
            if (state.TimeSinceLastSpawn >= 5.0f)
            {
                // Spawn enemy...
                state.EnemiesSpawned++;
                state.TimeSinceLastSpawn = 0f;
            }
        }
    }
}
```

#### ⚠️ Component Definitions Can't Change
Changing component struct layouts during hot reload will cause **crashes** or **data corruption**.

**DON'T DO THIS:**
```csharp
// Before reload:
public struct PlayerData
{
    public int Health;
    public int Score;
}

// After reload - WRONG! Memory layout changed!
public struct PlayerData
{
    public float Health;  // Changed type
    public int Score;
    public int Level;     // Added field
}
```

If you need to change component layout:
1. Stop the application
2. Modify component definition
3. Rebuild and restart

#### ✅ Safe Changes During Hot Reload
- System logic (OnCreate/OnUpdate/OnDestroy)
- Adding/removing systems
- Adding new component types (not modifying existing ones)
- Changing query logic
- Modifying algorithms

### Best Practices

#### 1. Minimize System State
```csharp
// BAD: State in system
public class BadSystem : ISystem
{
    private List<Enemy> _enemies = new();  // Lost on reload
}

// GOOD: State in ECS
public class GoodSystem : ISystem
{
    public void OnCreate(World world)
    {
        // Create entities to hold state
        var manager = world.CreateEntity();
        manager.AddComponent(new EnemyManager { Enemies = new List<EnemyId>() });
    }
}
```

#### 2. Use OnCreate for Initialization
```csharp
public class MySystem : ISystem
{
    public void OnCreate(World world)
    {
        // Register components
        ComponentRegistry.Register<MyComponent>();
        
        // Create singleton entities for system state
        var config = world.CreateEntity();
        config.AddComponent(new SystemConfig { ... });
    }
}
```

#### 3. Handle Missing Data Gracefully
```csharp
public void OnUpdate(World world, float deltaTime)
{
    // Check if required entities exist (might be destroyed during reload)
    foreach (var (entity, data) in world.Query<GameState>())
    {
        if (!entity.IsAlive)
        {
            continue; // Skip dead entities
        }
        
        // Process...
    }
}
```

#### 4. Avoid Static State
```csharp
// BAD: Static state persists across reloads incorrectly
public class BadSystem : ISystem
{
    private static int s_globalCounter = 0;  // ❌ Won't reset on reload
}

// GOOD: Instance state (gets reset) or ECS state (persists correctly)
public class GoodSystem : ISystem
{
    private int _instanceCounter = 0;  // ✅ Resets on reload
}
```

### Debugging Hot Reload

Enable console output to see hot reload events:
```
[ScriptFileWatcher] Detected change: Changed - MySystem.cs
[ScriptFileWatcher] Processing 1 changed file(s)
[ScriptHost] Unloading assembly: GameScripts.dll
[ScriptHost] Removed system instance: handle=1
[ScriptHost] Unloaded context for: GameScripts.dll
[ScriptHost] Successfully unloaded: GameScripts.dll
[ScriptHost] Loading assembly: GameScripts.dll
[ScriptHost] Loaded: GameScripts, Version=1.0.0.0
[ScriptHost] Discovering systems in loaded assemblies...
[ScriptHost] Found 3 system types
[ScriptHost] Created system instance: MySystem (handle=5)
```

### Manual Hot Reload Trigger

You can manually trigger hot reload from C++:
```cpp
scriptManager.ReloadAssembly("GameScripts.dll");
```

Or from C#:
```csharp
ScriptHost.ReloadAssembly("GameScripts.dll");
```

---

## Example: Complete Hot Reload Workflow

### 1. Initial System
```csharp
// MySystem.cs
public class MySystem : ISystem
{
    public void OnCreate(World world)
    {
        Console.WriteLine("MySystem created");
    }
    
    public void OnUpdate(World world, float deltaTime)
    {
        Console.WriteLine("MySystem updating");
    }
}
```

### 2. Edit While Running
```csharp
// MySystem.cs - Modified
public class MySystem : ISystem
{
    public void OnCreate(World world)
    {
        Console.WriteLine("MySystem created - Version 2!");
    }
    
    public void OnUpdate(World world, float deltaTime)
    {
        Console.WriteLine("MySystem updating - New behavior!");
        
        // Add new logic
        foreach (var (entity, pos) in world.Query<Position>())
        {
            pos.Value.Y += 10.0f * deltaTime;
        }
    }
}
```

### 3. File Watcher Detects Change
```
[ScriptFileWatcher] Detected change: Changed - MySystem.cs
```

### 4. Automatic Reload
```
[ScriptHost] Unloading assembly: GameScripts.dll
[ScriptHost] Loading assembly: GameScripts.dll
[ScriptHost] Created system instance: MySystem (handle=2)
```

### 5. New System Active
```
MySystem created - Version 2!
MySystem updating - New behavior!
```

---

## Troubleshooting

### Assembly Won't Unload
**Symptom**: Old code still executes after reload

**Cause**: Strong references to old assembly preventing GC

**Solution**: Ensure no static references to system instances

### Crash After Component Change
**Symptom**: Application crashes during/after reload

**Cause**: Component struct layout changed

**Solution**: Restart application, don't hot reload component definitions

### File Watcher Not Triggering
**Symptom**: Changes not detected

**Check**:
1. File watcher is started: `ScriptFileWatcher.StartWatching()`
2. Directory path is correct
3. File extension is `.cs`
4. Debounce timer hasn't stalled

### Memory Leak
**Symptom**: Memory grows after multiple reloads

**Cause**: Old assembly not fully unloading

**Solution**:
1. Ensure `AssemblyLoadContext` is collectible
2. No static references to types from collectible context
3. Call `GC.Collect()` after unload
