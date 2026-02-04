using System;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
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
    public int IdleStartFrame;
    public int IdleFrameCount;
    public float IdleFps;
    public int AttackStartFrame;
    public int AttackFrameCount;
    public float AttackFps;
    public bool AttackLoop;
    public BarnacleState State;
    public bool IsInRange;
    public int OverlapCount;
}

public sealed class BarnacleSnatcherTriggerSystem : TriggerSystemBase
{
    protected override void OnTriggerEnter(Entity self, TriggerEvent evt)
    {
        if (!self.HasComponent<BarnacleSnatcherComponent>())
            return;

        ref var ai = ref self.GetComponent<BarnacleSnatcherComponent>();
        ai.OverlapCount++;
        ai.IsInRange = ai.OverlapCount > 0;
    }

    protected override void OnTriggerExit(Entity self, TriggerExitEvent evt)
    {
        if (!self.HasComponent<BarnacleSnatcherComponent>())
            return;

        ref var ai = ref self.GetComponent<BarnacleSnatcherComponent>();
        if (ai.OverlapCount > 0)
            ai.OverlapCount--;
        ai.IsInRange = ai.OverlapCount > 0;
    }
}

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public sealed class BarnacleSnatcherStateSystem : SystemBase
{
    private bool _logged;

    protected override void OnUpdate()
    {
        if (!_logged)
        {
            _logged = true;
            System.Console.WriteLine("[BarnacleSnatcherStateSystem] Update running");
        }

        foreach (var result in World!.Query<BarnacleSnatcherComponent, SpriteSheetAnimation2D, AnimationState2D>())
        {
            ref var ai = ref result.Component1;
            ref var anim = ref result.Component2;
            ref var state = ref result.Component3;

            if (ai.State == BarnacleState.Attack)
            {
                if (state.Finished)
                {
                    if (ai.IsInRange)
                    {
                        SetAttack(ref ai, ref anim, ref state);
                    }
                    else
                    {
                        SetIdle(ref ai, ref anim, ref state);
                    }
                }
                else if (!ai.IsInRange && ai.AttackLoop)
                {
                    SetIdle(ref ai, ref anim, ref state);
                }
            }
            else
            {
                if (ai.IsInRange)
                {
                    SetAttack(ref ai, ref anim, ref state);
                }
                else
                {
                    EnsureIdle(ref ai, ref anim, ref state);
                }
            }
        }
    }

    private static void SetAttack(ref BarnacleSnatcherComponent ai, ref SpriteSheetAnimation2D anim, ref AnimationState2D state)
    {
        ai.State = BarnacleState.Attack;
        anim.StartFrame = ai.AttackStartFrame;
        anim.FrameCount = Math.Max(1, ai.AttackFrameCount);
        anim.FramesPerSecond = ai.AttackFps > 0.0f ? ai.AttackFps : anim.FramesPerSecond;
        anim.Loop = ai.AttackLoop;
        anim.Playing = true;
        anim.UseRow = false;

        state.CurrentFrame = 0;
        state.TimeAccumulator = 0.0f;
        state.Finished = false;
    }

    private static void SetIdle(ref BarnacleSnatcherComponent ai, ref SpriteSheetAnimation2D anim, ref AnimationState2D state)
    {
        ai.State = BarnacleState.Idle;
        anim.StartFrame = ai.IdleStartFrame;
        anim.FrameCount = Math.Max(1, ai.IdleFrameCount);
        anim.FramesPerSecond = ai.IdleFps > 0.0f ? ai.IdleFps : anim.FramesPerSecond;
        anim.Loop = true;
        anim.Playing = true;
        anim.UseRow = false;

        state.CurrentFrame = 0;
        state.TimeAccumulator = 0.0f;
        state.Finished = false;
    }

    private static void EnsureIdle(ref BarnacleSnatcherComponent ai, ref SpriteSheetAnimation2D anim, ref AnimationState2D state)
    {
        if (ai.State != BarnacleState.Idle ||
            anim.StartFrame != ai.IdleStartFrame ||
            anim.FrameCount != Math.Max(1, ai.IdleFrameCount) ||
            !anim.Loop)
        {
            SetIdle(ref ai, ref anim, ref state);
        }
    }
}
