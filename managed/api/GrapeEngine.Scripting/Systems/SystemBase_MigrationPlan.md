# SystemBase Migration Plan

## Purpose

This document defines a recommended migration from the current `ISystem` interface-style systems to an opt-in `SystemBase` concrete base class (Unity-like) in the managed scripting API. It includes exact C# method signatures, registration/shim ideas for native integration, and migration steps.

## Goals

- Provide an ergonomic, safe, and performant base class for most game systems.
- Preserve backwards compatibility for existing `ISystem` implementations.
- Integrate cleanly with the existing `Job` / `JobHandle` scheduling and the engine's system dependency graph.
- Make query caching, safe structural-change handling, and dependency propagation straightforward.

## High-level design

- Keep the `ISystem` interface as the minimal contract used by the engine to call into managed systems.
- Add an abstract `SystemBase` class that implements `ISystem` and provides common lifecycle hooks, query helpers, and scheduling helpers.
- Provide a small `SystemAdapter` shim to let the engine treat both `ISystem` and `SystemBase` uniformly.

## Managed API: exact signatures

Notes:
- Types referenced below (e.g., `World`, `JobHandle`, `EntityQuery`, `QueryEnumerator`) are assumed to exist in the managed API. Adjust namespace/type names to match your codebase.

### ISystem (existing-compatible contract)

```csharp
namespace GrapeEngine.Scripting.Systems
{
    // Minimal contract used by engine to call managed systems.
    public interface ISystem
    {
        // Called once when the system is created/registered.
        void OnCreate(in SystemCreateContext ctx);

        // Called each frame (or when scheduled). Return a JobHandle representing
        // scheduled work; return default(JobHandle) for no work.
        JobHandle OnUpdate(in SystemUpdateContext ctx);

        // Called once when the system is destroyed/unregistered.
        void OnDestroy(in SystemDestroyContext ctx);
    }
}
```

### System lifecycle context structs

```csharp
public readonly struct SystemCreateContext
{
    public readonly World World;
    public readonly ISystemMetadata Metadata;
}

public readonly struct SystemUpdateContext
{
    public readonly World World;
    public readonly float DeltaTime;
    // Input dependency of prior scheduled work; system should chain and return a JobHandle
    public readonly JobHandle Dependency;
}

public readonly struct SystemDestroyContext
{
    public readonly World World;
}
```

### SystemBase (recommended base class)

```csharp
public abstract class SystemBase : ISystem
{
    // Engine sets these during OnCreate
    public World World { get; private set; }
    public ISystemMetadata Metadata { get; private set; }

    // Internal initialization called by adapter/registration code
    void ISystem.OnCreate(in SystemCreateContext ctx)
    {
        World = ctx.World;
        Metadata = ctx.Metadata;
        OnCreate();
    }

    // Optional override: called once for one-time setup (cache queries, allocate buffers)
    protected virtual void OnCreate() { }

    // Implementers override this. They should return any JobHandle they schedule, or default.
    JobHandle ISystem.OnUpdate(in SystemUpdateContext ctx)
    {
        return OnUpdate(ctx);
    }

    // New per-frame API: return chained JobHandle representing work scheduled by the system.
    protected abstract JobHandle OnUpdate(in SystemUpdateContext ctx);

    void ISystem.OnDestroy(in SystemDestroyContext ctx)
    {
        OnDestroy();
        World = null;
        Metadata = null;
    }

    protected virtual void OnDestroy() { }

    // Helper: create/get cached EntityQuery (thread-safe from main thread)
    protected EntityQuery GetEntityQuery(params ComponentType[] types)
    {
        // Implement caching logic inside SystemBase (store in a Dictionary)
        throw new NotImplementedException();
    }

    // Helper: iterate query (convenience wrapper returning QueryEnumerator)
    protected QueryEnumerator<T1> QueryFor<T1>(EntityQuery query)
        where T1 : struct
    {
        throw new NotImplementedException();
    }

    // Helper: schedule a job and return resulting JobHandle
    protected JobHandle Schedule(Job job, JobHandle dependsOn = default)
    {
        return JobManager.Schedule(job, dependsOn);
    }
}
```

### Optional: convenience `ComponentSystem` variant with synchronous update

```csharp
public abstract class ComponentSystem : SystemBase
{
    protected sealed override JobHandle OnUpdate(in SystemUpdateContext ctx)
    {
        OnUpdateSync(ctx.DeltaTime);
        return default;
    }

    protected abstract void OnUpdateSync(float deltaTime);
}
```

### EntityQuery / Query helpers (signatures)

```csharp
public sealed class EntityQuery { }

public readonly ref struct QueryEnumerator<T> where T : struct
{
    public bool MoveNext();
    public ref readonly T Current { get; }
}

// Example helper signature to create typed queries
protected EntityQuery GetQuery<T1, T2>()
    where T1 : struct
    where T2 : struct;
```

## Native/Engine integration signatures (shims)

The engine core expects a minimal ABI for managed systems. Use a small adapter to translate between engine calls and the managed `ISystem`.

### SystemAdapter (managed shim)

```csharp
internal sealed class SystemAdapter
{
    private readonly ISystem _system;

    public SystemAdapter(ISystem system) => _system = system;

    // Called from native host; convert raw pointers into managed contexts
    public void Native_OnCreate(IntPtr worldPtr, IntPtr metadataPtr)
    {
        var ctx = new SystemCreateContext { World = World.FromNative(worldPtr), Metadata = SystemMetadata.FromNative(metadataPtr) };
        _system.OnCreate(in ctx);
    }

    public JobHandle Native_OnUpdate(float dt, JobHandle dependency, IntPtr worldPtr)
    {
        var ctx = new SystemUpdateContext { World = World.FromNative(worldPtr), DeltaTime = dt, Dependency = dependency };
        return _system.OnUpdate(in ctx);
    }

    public void Native_OnDestroy(IntPtr worldPtr)
    {
        var ctx = new SystemDestroyContext { World = World.FromNative(worldPtr) };
        _system.OnDestroy(in ctx);
    }
}
```

### Native exports / pinvoke surface (example signature ideas)

Use a single exported function per managed system instance, or register a function table. Example attributes:

```csharp
[UnmanagedCallersOnly(EntryPoint = "ManagedSystem_CreateAdapter")]
public static IntPtr ManagedSystem_CreateAdapter(IntPtr systemFactoryFuncPtr);

// Engine calls these exported functions passing adapter pointer
[UnmanagedCallersOnly(EntryPoint = "ManagedSystem_Adapter_OnCreate")]
public static void ManagedSystem_Adapter_OnCreate(IntPtr adapter, IntPtr world, IntPtr metadata);

[UnmanagedCallersOnly(EntryPoint = "ManagedSystem_Adapter_OnUpdate")]
public static JobHandle ManagedSystem_Adapter_OnUpdate(IntPtr adapter, float dt, JobHandle dependsOn, IntPtr world);

[UnmanagedCallersOnly(EntryPoint = "ManagedSystem_Adapter_OnDestroy")]
public static void ManagedSystem_Adapter_OnDestroy(IntPtr adapter, IntPtr world);
```

Adjust exact P/Invoke/embedding approach to match your current host (nethost, native host and how managed assemblies are loaded). The `Unsafe/` and `Hosting/` folders in the repo contain similar patterns to follow.

## Registration & discovery

- Provide a `SystemRegistry` API to register managed systems, either by type or by instance.

```csharp
public static class SystemRegistry
{
    public static void Register<TSystem>() where TSystem : ISystem, new();
    public static void RegisterInstance(ISystem system);
}
```

- During engine startup the native side enumerates managed-registered systems and creates native nodes in the SystemDependencyGraph.

## Migration steps (detailed)

1. Add `SystemBase` and supporting context types to `managed/api/GrapeEngine.Scripting/Systems/`.
2. Implement `SystemAdapter` and native export functions to bridge engine calls to `ISystem`.
3. Implement `SystemRegistry` and a discovery path that mirrors your current component discovery (see `Hosting/ComponentDiscovery.cs`).
4. Provide query-caching helpers inside `SystemBase` (use a Dictionary<ComponentType[] -> EntityQuery> keyed by a stable representation).
5. Provide clear guidance for structural-change rules: by default, structural changes should be deferred or scheduled via the `World` API; add runtime asserts in debug builds.
6. Add unit/integration tests:
   - Validate a `SystemBase` system runs and returns `JobHandle` that composes with `JobManager`.
   - Validate query caching is reused and does not allocate each frame.
   - Validate structural-change deferral prevents iterator invalidation.
7. Add profiling counters and sample systems to compare old `ISystem` implementations vs `SystemBase` performance.
8. Document migration for users (example before/after implementations).

## Example migration (before/after)

Before (existing `ISystem` implementor):

```csharp
public class MySystem : ISystem
{
    public void OnCreate(in SystemCreateContext ctx) { /* ... */ }
    public JobHandle OnUpdate(in SystemUpdateContext ctx) { /* ... */ }
    public void OnDestroy(in SystemDestroyContext ctx) { /* ... */ }
}
```

After (recommended):

```csharp
public class MySystem : SystemBase
{
    protected override void OnCreate() { /* cache queries */ }
    protected override JobHandle OnUpdate(in SystemUpdateContext ctx)
    {
        // schedule jobs or use Query helpers
        return default;
    }
    protected override void OnDestroy() { /* cleanup */ }
}
```

## Rollout strategy

- Phase 1: Add `SystemBase` and adapter; keep `ISystem` intact. Offer `SystemBase` as opt-in for new systems.
- Phase 2: Encourage migration of non-performance-critical systems; provide examples and automated refactors if possible.
- Phase 3: Measure & optimize query helpers and job chaining. If needed, add advanced APIs for low-level control.

## Risks and mitigation

- Risk: `SystemBase` hides low-level control and causes misuse leading to suboptimal scheduling.
  - Mitigation: provide advanced examples and document how to escape abstraction using `ISystem` directly.
- Risk: Structural changes from systems can invalidate iterators.
  - Mitigation: default to deferring structural changes and expose explicit immediate APIs with assertions.

## Next steps (concrete tasks)

1. Add code scaffolding for `SystemBase`, `SystemCreateContext`, `SystemUpdateContext`, `SystemDestroyContext`.
2. Implement `SystemAdapter` and example native exports matching the host pattern.
3. Add `SystemRegistry` and discovery integration.
4. Write example conversions for 2-3 existing systems and add tests.

---

File created by migration plan generator. If you want, I can now scaffold `SystemBase` and the adapter code in the repository (create `.cs` files with the signatures above and implement the basic caching and adapters).
