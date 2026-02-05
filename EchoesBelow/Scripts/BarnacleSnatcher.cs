using System;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using GrapeEngine.Scripting.Gameplay;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

public enum BarnacleState : byte
{
    Idle = 0,
    Attack = 1
}

[Component(Name = "Barnacle Snatcher")]
public record struct BarnacleSnatcherComponent
{
    public int IdleStartFrame = 21;
    public int IdleFrameCount = 58;
    public float IdleFps = 24.0f;
    public int AnimRow = 26;
    public int IdleFrameOffset = 0;
    public int IdleFrameLength = 60;
    public int AttackStartFrame = 60;
    public int AttackFrameCount = 18;
    public float AttackFps = 24.0f;
    public int AttackFrameOffset = 0;
    public int AttackFrameLength = 18;
    public bool AttackLoop = false;
    public bool Initialized = false;
    public BarnacleState State = BarnacleState.Idle;
    public int AnimFrameIndex = 0;
    public float AnimTimeAccumulator = 0.0f;
    public bool IsInRange = false;
    public int OverlapCount = 0;
    public ulong BrainHandle = 0;
    public float AttackCooldownTimer = 0.0f;

    public BarnacleSnatcherComponent() { }
}

public sealed class BarnacleSnatcherTriggerSystem : TriggerSystemBase
{
    private const string BarnacleName = "Barn Snatch";
    private const string SquidwardColliderName = "Collider";
    private static readonly string[] BarnacleColliderNames = { "Barn Snatch", "Attack AOE collider" };

    protected override void OnTriggerEnter(Entity self, TriggerEvent evt)
    {
        if (!TryResolveBarnacle(World!, self, evt.OtherEntityId, out var barnacle))
            return;

        ref var ai = ref barnacle.GetComponent<BarnacleSnatcherComponent>();
        ai.OverlapCount++;
        ai.IsInRange = ai.OverlapCount > 0;
    }

    protected override void OnTriggerExit(Entity self, TriggerExitEvent evt)
    {
        if (!TryResolveBarnacle(World!, self, evt.OtherEntityId, out var barnacle))
            return;

        ref var ai = ref barnacle.GetComponent<BarnacleSnatcherComponent>();
        if (ai.OverlapCount > 0)
            ai.OverlapCount--;
        ai.IsInRange = ai.OverlapCount > 0;
    }

    private static bool TryResolveBarnacle(World world, Entity self, ulong otherId, out Entity barnacle)
    {
        barnacle = default;
        if (!self.IsAlive)
            return false;

        Entity other = Entity.FromId(world, otherId);
        if (!other.IsAlive)
            return false;

        string selfName = GetName(self);
        string otherName = GetName(other);

        bool selfIsSquidCollider = IsName(selfName, SquidwardColliderName);
        bool otherIsSquidCollider = IsName(otherName, SquidwardColliderName);
        bool selfIsBarnacleCollider = IsBarnacleCollider(selfName);
        bool otherIsBarnacleCollider = IsBarnacleCollider(otherName);

        if (!((selfIsSquidCollider && otherIsBarnacleCollider) ||
              (otherIsSquidCollider && selfIsBarnacleCollider)))
        {
            return false;
        }

        return TryFindBarnacle(world, out barnacle);
    }

    private static bool TryFindBarnacle(World world, out Entity barnacle)
    {
        barnacle = default;
        foreach (var result in world.Query<Name>())
        {
            string name = Strings.Resolve(result.Component1.Value) ?? string.Empty;
            if (IsName(name, BarnacleName))
            {
                barnacle = result.Entity;
                return barnacle.IsAlive && barnacle.HasComponent<BarnacleSnatcherComponent>();
            }
        }

        return false;
    }

    private static string GetName(Entity entity)
    {
        if (!entity.TryGetComponent<Name>(out var name))
            return string.Empty;

        return Strings.Resolve(name.Value) ?? string.Empty;
    }

    private static bool IsBarnacleCollider(string name)
    {
        if (name.Length == 0)
            return false;

        foreach (var candidate in BarnacleColliderNames)
        {
            if (IsName(name, candidate))
                return true;
        }

        return false;
    }

    private static bool IsName(string actual, string expected)
        => actual.Length != 0 && string.Equals(actual, expected, StringComparison.OrdinalIgnoreCase);
}

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public sealed class BarnacleSnatcherStateSystem : SystemBase
{
    private const float AttackCooldownSeconds = 3.0f;
    private static readonly Color IdleColor = Color.White;
    private static readonly Color AttackColor = Color.Red;
    private bool _logged;

    protected override void OnUpdate()
    {
        if (!_logged)
        {
            _logged = true;
            System.Console.WriteLine("[BarnacleSnatcherStateSystem] Update running");
        }

        foreach (var result in World!.Query<BarnacleSnatcherComponent, SpriteSheetAnimation2D, AnimationState2D, SpriteRenderer2D>())
        {
            ref var ai = ref result.Component1;
            ref var anim = ref result.Component2;
            ref var state = ref result.Component3;
            ref var renderer = ref result.Component4;
            EnsureDefaults(ref ai);
            Brain brain = GetOrCreateBrain(ref ai);

            brain.Update(Time.DeltaTime);

            if (ai.AttackCooldownTimer > 0.0f)
            {
                ai.AttackCooldownTimer = Math.Max(0.0f, ai.AttackCooldownTimer - Time.DeltaTime);
            }

            nint currentState = brain.GetCurrentState();
            nint attackState = brain.GetAttackState();
            nint patrolState = brain.GetPatrolState();
            bool isAttackState = currentState == attackState;
            bool canAttack = ai.IsInRange && ai.AttackCooldownTimer <= 0.0f;

            if (isAttackState)
            {
                if (state.Finished)
                {
                    ai.AttackCooldownTimer = AttackCooldownSeconds;
                    currentState = TransitionIfDifferent(brain, currentState, patrolState);
                }
                else if (!ai.IsInRange && ai.AttackLoop)
                {
                    currentState = TransitionIfDifferent(brain, currentState, patrolState);
                }
            }
            else
            {
                if (canAttack)
                {
                    currentState = TransitionIfDifferent(brain, currentState, attackState);
                }
            }

            isAttackState = currentState == attackState;
            if (isAttackState)
            {
                EnsureAttack(ref ai, ref anim, ref state);
                renderer.Color = AttackColor;
            }
            else
            {
                EnsureIdle(ref ai, ref anim, ref state);
                renderer.Color = IdleColor;
            }
        }
    }

    protected override void OnDestroy()
    {
        if (World == null)
        {
            return;
        }

        foreach (var result in World.Query<BarnacleSnatcherComponent>())
        {
            ref var ai = ref result.Component1;
            if (ai.BrainHandle == 0)
            {
                continue;
            }

            Brain.DestroyHandle((nint)ai.BrainHandle);
            ai.BrainHandle = 0;
        }
    }

    private static Brain GetOrCreateBrain(ref BarnacleSnatcherComponent ai)
    {
        if (ai.BrainHandle == 0)
        {
            Brain created = Brain.Create();
            ai.BrainHandle = (ulong)created.Handle;
            return created;
        }

        return Brain.FromHandle((nint)ai.BrainHandle);
    }

    private static nint TransitionIfDifferent(Brain brain, nint currentState, nint targetState)
    {
        if (currentState == targetState)
        {
            return currentState;
        }

        brain.TransitionTo(targetState);
        return targetState;
    }

    private static void SetAttack(ref BarnacleSnatcherComponent ai, ref SpriteSheetAnimation2D anim, ref AnimationState2D state)
    {
        ai.State = BarnacleState.Attack;
        anim.Row = ai.AnimRow;
        anim.FrameOffset = Math.Max(0, ai.AttackFrameOffset);
        anim.FrameLength = Math.Max(1, ai.AttackFrameLength);
        anim.Loop = ai.AttackLoop;
        anim.Playing = true;
        anim.UseRow = true;
        anim.FramesPerSecond = ai.AttackFps > 0.0f ? ai.AttackFps : anim.FramesPerSecond;

        state.CurrentFrame = 0;
        state.TimeAccumulator = 0.0f;
        state.Finished = false;
    }

    private static void SetIdle(ref BarnacleSnatcherComponent ai, ref SpriteSheetAnimation2D anim, ref AnimationState2D state)
    {
        ai.State = BarnacleState.Idle;
        anim.Row = ai.AnimRow;
        anim.FrameOffset = Math.Max(0, ai.IdleFrameOffset);
        anim.FrameLength = Math.Max(1, ai.IdleFrameLength);
        anim.Loop = true;
        anim.Playing = true;
        anim.UseRow = true;
        anim.FramesPerSecond = ai.IdleFps > 0.0f ? ai.IdleFps : anim.FramesPerSecond;

        state.CurrentFrame = 0;
        state.TimeAccumulator = 0.0f;
        state.Finished = false;
    }

    private static void EnsureIdle(ref BarnacleSnatcherComponent ai, ref SpriteSheetAnimation2D anim, ref AnimationState2D state)
    {
        if (ai.State != BarnacleState.Idle ||
            !anim.Loop ||
            !anim.UseRow ||
            anim.Row != ai.AnimRow ||
            anim.FrameOffset != Math.Max(0, ai.IdleFrameOffset) ||
            anim.FrameLength != Math.Max(1, ai.IdleFrameLength))
        {
            SetIdle(ref ai, ref anim, ref state);
        }
    }

    private static void EnsureAttack(ref BarnacleSnatcherComponent ai, ref SpriteSheetAnimation2D anim, ref AnimationState2D state)
    {
        if (ai.State != BarnacleState.Attack ||
            anim.Loop != ai.AttackLoop ||
            !anim.UseRow ||
            anim.Row != ai.AnimRow ||
            anim.FrameOffset != Math.Max(0, ai.AttackFrameOffset) ||
            anim.FrameLength != Math.Max(1, ai.AttackFrameLength))
        {
            SetAttack(ref ai, ref anim, ref state);
        }
    }

    private static void EnsureDefaults(ref BarnacleSnatcherComponent ai)
    {
        if (ai.Initialized)
            return;

        if (ai.IdleFrameCount <= 0 && ai.AttackFrameCount <= 0)
        {
            ai.IdleStartFrame = 21;
            ai.IdleFrameCount = 58;
            ai.IdleFps = 24.0f;
            ai.AttackStartFrame = 3;
            ai.AttackFrameCount = 18;
            ai.AttackFps = 19.0f;
            ai.AttackLoop = false;
            ai.State = BarnacleState.Idle;
        }

        if (ai.AnimRow < 0)
            ai.AnimRow = 26;

        if (ai.IdleFrameLength <= 0)
        {
            ai.IdleFrameOffset = 19;
            ai.IdleFrameLength = 60;
        }

        if (ai.AttackFrameLength <= 0)
        {
            ai.AttackFrameOffset = 0;
            ai.AttackFrameLength = 18;
        }

        ai.Initialized = true;
    }
}
