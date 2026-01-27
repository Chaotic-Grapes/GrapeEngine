using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using System.Collections.Generic;

namespace EchoesBelow.Scripts;

/// <summary>
/// Component representing the lifetime of an entity in seconds.
/// When Time <= 0, the entity can be destroyed by the LifetimeSystem.
/// </summary>
[Component]
public record struct LifetimeComponent(float Time, float MaxTime);

/// <summary>
/// System that decreases Lifetime and destroys expired entities.
/// Executes in Update phase with default execution order.
/// 
/// This system iterates over all entities with Lifetime components,
/// decreases their Time by deltaTime each frame, and destroys entities
/// when their lifetime expires (Time <= 0).
/// 
/// If an entity has an Active component, it is only updated when enabled.
/// Entities without the Active component are treated as enabled by default.
/// </summary>
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class LifetimeSystem : SystemBase
{
    // One-time initialization
    protected override void OnCreate()
    {
        // No initialization needed for LifetimeSystem
        Log("This is a test", LogLevel.Warning);
        Log(() => "[LifetimeSystem] Created", LogLevel.Debug);
    }

    // Called every frame. Updates entity lifetimes and destroys expired entities.
    protected override void OnUpdate()
    {
        var deltaTime = Time.DeltaTime;
        var entitiesToDestroy = new List<Entity>();

        // Query all entities with Lifetime and Active components
        foreach (var result in World!.Query<LifetimeComponent, Active>())
        {
            var entity = result.Entity;
            ref var lifetime = ref result.Component1;
            ref var active = ref result.Component2;

            // Skip processing if entity is disabled
            if (!active.Enabled)
                continue;

            // Decrease lifetime in-place via ref
            lifetime.Time -= deltaTime;

            // Mark for destruction if lifetime expired
            if (lifetime.Time <= 0.0f)
            {
                entitiesToDestroy.Add(entity);
            }
        }

        // Process entities with Lifetime but NO Active component (treat as enabled)
        foreach (var result in World!.Query<LifetimeComponent>().Without<Active>())
        {
            var entity = result.Entity;
            ref var lifetime = ref result.Component1;

            // Decrease lifetime in-place via ref
            lifetime.Time -= deltaTime;

            // Mark for destruction if lifetime expired
            if (lifetime.Time <= 0.0f)
            {
                entitiesToDestroy.Add(entity);
            }
        }

        // Batch destroy all expired entities instead of destroying individually
        // This reduces entity removal overhead by batching structural changes
        if (entitiesToDestroy.Count > 0)
        {
            foreach (var entity in entitiesToDestroy)
            {
                World!.DestroyEntity(entity);
            }
        }
    }

    // Cleanup
    protected override void OnDestroy()
    {
        // No cleanup needed for LifetimeSystem
        Log("[LifetimeSystem] Destroyed", LogLevel.Debug);
    }
}

