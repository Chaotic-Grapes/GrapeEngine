using System;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

public enum SquidwardState : byte
{
    Idle = 0,
    Attack = 1
}

[Component(Name = "Squidward Attack")]
public record struct SquidwardAttackComponent
{
    public SquidwardState State;
    public bool InRange;
    public float CooldownTimer;
    public float AttackTimer;
}

public sealed class SquidwardAttackTriggerSystem : TriggerSystemBase
{
    private const string SquidwardName = "squidward";
    private const string SquidwardColliderName = "Collider";
    private static readonly string[] BarnacleColliderNames = { "Barn Snatch", "Attack AOE collider" };

    protected override void OnTriggerEnter(Entity self, TriggerEvent evt)
    {
        if (!TryResolveRange(World!, self, evt.OtherEntityId, out var squidward))
            return;

        ref var ai = ref EnsureComponent(squidward);
        ai.InRange = true;
    }

    protected override void OnTriggerExit(Entity self, TriggerExitEvent evt)
    {
        if (!TryResolveRange(World!, self, evt.OtherEntityId, out var squidward))
            return;

        ref var ai = ref EnsureComponent(squidward);
        ai.InRange = false;
    }

    private static bool TryResolveRange(World world, Entity self, ulong otherId, out Entity squidward)
    {
        squidward = default;
        if (!self.IsAlive)
            return false;

        Entity other = Entity.FromId(world, otherId);
        if (!other.IsAlive)
            return false;

        string? selfName = GetName(self);
        string? otherName = GetName(other);

        bool selfIsSquidCollider = IsName(selfName, SquidwardColliderName);
        bool otherIsSquidCollider = IsName(otherName, SquidwardColliderName);
        bool selfIsBarnacleCollider = IsBarnacleCollider(selfName);
        bool otherIsBarnacleCollider = IsBarnacleCollider(otherName);

        if (!((selfIsSquidCollider && otherIsBarnacleCollider) ||
              (otherIsSquidCollider && selfIsBarnacleCollider)))
        {
            return false;
        }

        var squidwardEntity = FindEntityByName(world, SquidwardName);
        if (squidwardEntity == null || !squidwardEntity.Value.IsAlive)
            return false;

        squidward = squidwardEntity.Value;
        return true;
    }

    private static ref SquidwardAttackComponent EnsureComponent(Entity entity)
    {
        if (!entity.HasComponent<SquidwardAttackComponent>())
        {
            entity.AddComponent(new SquidwardAttackComponent
            {
                State = SquidwardState.Idle,
                InRange = false,
                CooldownTimer = 0.0f,
                AttackTimer = 0.0f
            });
        }

        return ref entity.GetComponent<SquidwardAttackComponent>();
    }

    private static Entity? FindEntityByName(World world, string name)
    {
        foreach (var result in world.Query<Name>())
        {
            var entity = result.Entity;
            string? entityName = Strings.Resolve(result.Component1.Value);
            if (IsName(entityName, name))
                return entity;
        }

        return null;
    }

    private static string? GetName(Entity entity)
    {
        if (!entity.TryGetComponent<Name>(out var name))
            return null;

        return Strings.Resolve(name.Value);
    }

    private static bool IsBarnacleCollider(string? name)
    {
        if (name == null)
            return false;

        foreach (var candidate in BarnacleColliderNames)
        {
            if (IsName(name, candidate))
                return true;
        }

        return false;
    }

    private static bool IsName(string? actual, string expected)
        => actual != null && string.Equals(actual, expected, StringComparison.OrdinalIgnoreCase);
}

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public sealed class SquidwardAttackStateSystem : SystemBase
{
    private const float AttackCooldownSeconds = 3.0f;
    private const float AttackDurationSeconds = 0.5f;
    private static readonly Color IdleColor = new(1f, 1f, 1f, 1f);
    private static readonly Color AttackColor = new(1f, 0f, 0f, 1f);

    protected override void OnUpdate()
    {
        if (!EnsureSquidwardComponent())
            return;

        foreach (var result in World!.Query<SquidwardAttackComponent, SpriteRenderer2D>())
        {
            ref var ai = ref result.Component1;
            ref var sprite = ref result.Component2;

            if (ai.CooldownTimer > 0.0f)
                ai.CooldownTimer = Math.Max(0.0f, ai.CooldownTimer - Time.DeltaTime);

            if (ai.AttackTimer > 0.0f)
                ai.AttackTimer = Math.Max(0.0f, ai.AttackTimer - Time.DeltaTime);

            if (ai.State == SquidwardState.Attack && ai.AttackTimer <= 0.0f)
                ai.State = SquidwardState.Idle;

            if (ai.State != SquidwardState.Attack && ai.InRange && ai.CooldownTimer <= 0.0f)
            {
                ai.State = SquidwardState.Attack;
                ai.AttackTimer = AttackDurationSeconds;
                ai.CooldownTimer = AttackCooldownSeconds;
            }

            sprite.Color = ai.State == SquidwardState.Attack ? AttackColor : IdleColor;
        }
    }

    private bool EnsureSquidwardComponent()
    {
        if (World == null)
            return false;

        foreach (var result in World.Query<Name>())
        {
            if (!string.Equals(Strings.Resolve(result.Component1.Value), "squidward", StringComparison.OrdinalIgnoreCase))
                continue;

            var entity = result.Entity;
            if (!entity.HasComponent<SquidwardAttackComponent>())
            {
                entity.AddComponent(new SquidwardAttackComponent
                {
                    State = SquidwardState.Idle,
                    InRange = false,
                    CooldownTimer = 0.0f,
                    AttackTimer = 0.0f
                });
            }
            return true;
        }

        return false;
    }
}
