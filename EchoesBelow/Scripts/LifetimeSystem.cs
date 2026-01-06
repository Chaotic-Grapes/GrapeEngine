using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Hosting;
using GrapeEngine.Scripting.Components.Core;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace EchoesBelow.Scripts;

/// <summary>
/// Component representing the lifetime of an entity in seconds.
/// When Time <= 0, the entity can be destroyed by the LifetimeSystem.
/// </summary>
[Component]
public record struct LifetimeComponent(float Time);

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
[SystemGroup(SystemGroup.Update)]
public class LifetimeSystem : SystemBase
{
    // One-time initialization
    protected override void OnCreate()
    {
        // No initialization needed for LifetimeSystem
        Log("[LifetimeSystem] Created", LogLevel.Debug);
    }

    // Called every frame. Updates entity lifetimes and destroys expired entities.
    protected override void OnUpdate()
    {
        var deltaTime = Time.DeltaTime;
        var entitiesToDestroy = new List<Entity>();

        Log($"[LifetimeSystem] Updating lifetimes with deltaTime={deltaTime}", LogLevel.Debug);

        // Query all entities with Lifetime components (use ref-local for in-place updates)
        foreach (var result in World!.Query<LifetimeComponent>())
        {
            var entity = result.Entity;
            ref var lifetime = ref result.Component1;

            // Check if entity is enabled. If ActiveComponent doesn't exist,
            // treat the entity as enabled by default.
            bool isEnabled = true;
            if (entity.HasComponent<Active>())
            {
                var active = entity.GetComponent<Active>();
                isEnabled = active.Enabled;
            }

            // Skip processing if entity is disabled
            if (!isEnabled)
                continue;

            // Decrease lifetime in-place via ref
            lifetime.Time -= deltaTime;

            // Mark for destruction if lifetime expired
            if (lifetime.Time <= 0.0f)
            {
                entitiesToDestroy.Add(entity);
            }
        }

        // Destroy all expired entities
        foreach (var entity in entitiesToDestroy)
        {
            World!.DestroyEntity(entity);
        }
    }

    // Cleanup
    protected override void OnDestroy()
    {
        // No cleanup needed for LifetimeSystem
        Log("[LifetimeSystem] Destroyed", LogLevel.Debug);
    }
}
