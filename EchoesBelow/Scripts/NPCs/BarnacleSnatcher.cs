/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu
 * @file    BarnacleSnatcher.cs
 * 
 * @brief   Barnacle snatcher AI, animation, and collision handling.
 */

using System;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using GrapeEngine.Scripting.Gameplay;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

// Simple AI states for the barnacle.
public enum BarnacleState : byte
{
    Idle = 0,
    Attack = 1
}

[Component(Name = "Barnacle Snatcher")]
// Per-entity config + runtime state for the barnacle snatcher.
public record struct BarnacleSnatcherComponent
{
    // Idle animation config.
    public int IdleStartFrame = 0;
    public int IdleFrameCount = 20;
    public float IdleFps = 30.0f;
    public int AnimRow = 0;
    public int IdleFrameOffset = 0;
    public int IdleFrameLength = 20;
    // Attack animation config.
    public int AttackStartFrame = 1;
    public int AttackFrameCount = 20;
    public float AttackFps = 24.0f;
    public int AttackFrameOffset = 1;
    public int AttackFrameLength = 20;
    public bool AttackLoop = false;
    // Runtime animation/AI state.
    public bool Initialized = false;
    public BarnacleState State = BarnacleState.Idle;
    public int AnimFrameIndex = 0;
    public float AnimTimeAccumulator = 0.0f;
    public bool IsInRange = false;
    public int OverlapCount = 0;
    public ulong BrainHandle = 0;
    public float AttackCooldownTimer = 0.0f;
    public bool AwaitingExit = false;
    // Debug state.
    public bool DebugLogs = false;
    public BarnacleState LastState = BarnacleState.Idle;
    public int LastAnimRow = -1;
    public int LastFrameOffset = -1;
    public int LastFrameLength = -1;
    public float DebugLogTimer = 0.0f;

    public BarnacleSnatcherComponent() { }
}

//// Tracks collider overlap between the barnacle and squidward.
//public sealed class BarnacleSnatcherTriggerSystem : CollisionSystemBase
//{
//    private const string BarnacleName = "Barnacle Snatcher";
//    private const string SquidwardName = "NPCInteractor_squidward";

//    protected override void OnCollisionEnter(Entity self, CollisionEvent evt)
//    {
//        return;
//        //Log("SNATCH COLLISION entered");
//        LogCollision(World!, "Enter", self, evt.OtherEntityId);
//        if (!TryResolveBarnacle(World!, self, evt.OtherEntityId, out var barnacle))
//            return;

//        // Mark target in range.
//        ref var ai = ref barnacle.GetComponent<BarnacleSnatcherComponent>();
//        ai.OverlapCount++;
//        ai.IsInRange = ai.OverlapCount > 0;
//    }

//    protected override void OnCollisionExit(Entity self, CollisionExitEvent evt)
//    {
//        return;
//        //Log("SNATCH COLLISION exit");
//        LogCollision(World!, "Exit", self, evt.OtherEntityId);
//        if (!TryResolveBarnacle(World!, self, evt.OtherEntityId, out var barnacle))
//            return;

//        // Decrement overlap count on exit.
//        ref var ai = ref barnacle.GetComponent<BarnacleSnatcherComponent>();
//        if (ai.OverlapCount > 0)
//            ai.OverlapCount--;
//        ai.IsInRange = ai.OverlapCount > 0;
//    }

//    // Resolve which entity is the barnacle for a barnacle/squidward collision.
//    private static bool TryResolveBarnacle(World world, Entity self, ulong otherId, out Entity barnacle)
//    {
//        barnacle = default;
//        if (!self.IsAlive)
//            return false;

//        Entity other = Entity.FromId(world, otherId);
//        if (!other.IsAlive)
//            return false;

//        string selfName = GetName(self);
//        string otherName = GetName(other);

//        bool selfIsBarnacle = IsName(selfName, BarnacleName);
//        bool otherIsBarnacle = IsName(otherName, BarnacleName);
//        bool selfIsSquidward = IsName(selfName, SquidwardName);
//        bool otherIsSquidward = IsName(otherName, SquidwardName);

//        if (!((selfIsBarnacle && otherIsSquidward) ||
//              (otherIsBarnacle && selfIsSquidward)))
//        {
//            return false;
//        }

//        return TryFindBarnacle(world, out barnacle);
//    }

//    // Find the barnacle entity by name.
//    private static bool TryFindBarnacle(World world, out Entity barnacle)
//    {
//        barnacle = default;
//        foreach (var result in world.Query<Name>())
//        {
//            string name = Strings.Resolve(result.Component1.Value) ?? string.Empty;
//            if (IsName(name, BarnacleName))
//            {
//                barnacle = result.Entity;
//                return barnacle.IsAlive && barnacle.HasComponent<BarnacleSnatcherComponent>();
//            }
//        }

//        return false;
//    }

//    // Get entity name if present.
//    private static string GetName(Entity entity)
//    {
//        if (!entity.TryGetComponent<Name>(out var name))
//            return string.Empty;

//        return Strings.Resolve(name.Value) ?? string.Empty;
//    }

//    // Simple name matcher.
//    private static bool IsName(string actual, string expected)
//        => actual.Length != 0 && string.Equals(actual, expected, StringComparison.OrdinalIgnoreCase);

//    // Debug logger for collisions involving barnacle or squidward.
//    private static void LogCollision(World world, string kind, Entity self, ulong otherId)
//    {
//        Entity other = Entity.FromId(world, otherId);
//        string selfName = GetName(self);
//        string otherName = other.IsAlive ? GetName(other) : "<dead>";
//        bool involvesBarnacle = IsName(selfName, BarnacleName) || IsName(otherName, BarnacleName);
//        bool involvesSquidward = IsName(selfName, SquidwardName) || IsName(otherName, SquidwardName);
//        if (!involvesBarnacle && !involvesSquidward)
//        {
//            return;
//        }
//        System.Console.WriteLine(
//            $"[BarnacleSnatcher] Collision {kind}: self='{selfName}' other='{otherName}'");
//    }
//}

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
// Main barnacle AI/animation update system.
public sealed class BarnacleSnatcherStateSystem : SystemBase
{
    // Sprite sheet metadata.
    private const string BarnacleTexturePath = "EchoesBelow/Assets/Sprites/NPCs/BarnacleSnatcher_SpriteSheet_Snatch_FullHorizontal.png";
    private const string BarnacleName = "Barnacle Snatcher";
    private const int BarnacleFrameWidth = 900;
    private const int BarnacleFrameHeight = 220;
    private const int BarnacleSheetWidth = 18900;
    private const int BarnacleSheetHeight = 220;
    private const int BarnacleFrameCount = 21;
    private const float AttackCooldownSeconds = 1.0f;
    // Debug colors.
    private static readonly Color IdleColor = Color.White;
    private static readonly Color AttackColor = Color.Red;
    // Shared logger state.
    private static StringId s_barnacleTexturePathId;
    private static bool s_metadataLogged;
    private static bool s_renderLogged;
    private static bool s_entityLogged;
    private static float s_modeLogTimer;
    private bool _logged;

    protected override void OnUpdate()
    {
        // One-time update marker.
        if (!_logged)
        {
            _logged = true;
            System.Console.WriteLine("[BarnacleSnatcherStateSystem] Update running");
        }

        // Fetch squidward transform/collider.
        bool hasSquidward = TryGetSquidwardData(World!, out var squidwardTransform, out var squidwardCollider);
        int barnacleCount = 0;
        // Update each barnacle with animation + AI components.
        foreach (var result in World!.Query<BarnacleSnatcherComponent, SpriteSheetAnimation2D, AnimationState2D, SpriteRenderer2D>())
        {
            barnacleCount++;
            ref var ai = ref result.Component1;
            ref var anim = ref result.Component2;
            ref var state = ref result.Component3;
            var renderer = result.Entity.GetComponent<SpriteRenderer2D>();
            // Ensure component defaults and sprite sheet metadata.
            EnsureDefaults(ref ai);
            EnsureSheetMetadata(ref anim, ref renderer);
            Brain brain = GetOrCreateBrain(ref ai);

            // Tick AI brain.
            brain.Update(Time.DeltaTime);

            // Cooldown timer tick.
            if (ai.AttackCooldownTimer > 0.0f)
            {
                ai.AttackCooldownTimer = Math.Max(0.0f, ai.AttackCooldownTimer - Time.DeltaTime);
            }

            // Start cooldown once squidward leaves.
            if (ai.AwaitingExit && !ai.IsInRange)
            {
                ai.AttackCooldownTimer = AttackCooldownSeconds;
                ai.AwaitingExit = false;
            }

            // Optional overlap check based on AABB.
            if (hasSquidward && result.Entity.TryGetComponent<LocalTransform>(out var barnacleTransform) &&
                result.Entity.TryGetComponent<BoxCollider2D>(out var barnacleCollider))
            {
                bool overlap = AabbOverlap(barnacleTransform, barnacleCollider, squidwardTransform, squidwardCollider);
                ai.IsInRange = overlap;
                ai.OverlapCount = overlap ? 1 : 0;
                LogAabb("Barnacle Snatcher", barnacleTransform, barnacleCollider, "squidward", squidwardTransform, squidwardCollider, overlap);
            }

            nint currentState = brain.GetCurrentState();
            nint attackState = brain.GetAttackState();
            nint patrolState = brain.GetPatrolState();
            bool isAttackState = currentState == attackState;
            bool canAttack = ai.IsInRange && !ai.AwaitingExit && ai.AttackCooldownTimer <= 0.0f;

            // Force patrol if target out of range.
            if (!ai.IsInRange)
            {
                currentState = TransitionIfDifferent(brain, currentState, patrolState);
                isAttackState = false;
            }

            // Attack state handling and transitions.
            if (isAttackState)
            {
                if (state.Finished)
                {
                    ai.AwaitingExit = true;
                    currentState = TransitionIfDifferent(brain, currentState, patrolState);
                }
                else if (!ai.IsInRange)
                {
                    currentState = TransitionIfDifferent(brain, currentState, patrolState);
                }
            }
            else
            {
                // Start attack if in range and ready.
                if (canAttack)
                {
                    currentState = TransitionIfDifferent(brain, currentState, attackState);
                }
            }

            // Apply animation state and debug color.
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

            // Always log the current mode for now.
            System.Console.WriteLine(
                $"[BarnacleSnatcher] Mode={ai.State} InRange={ai.IsInRange} Cooldown={ai.AttackCooldownTimer:0.00}");
            // Force renderer update with white.
            renderer.Color = new Color(1.0f, 1.0f, 1.0f, 1.0f);
            result.Entity.SetComponent(renderer);
            var storedRenderer = result.Entity.GetComponent<SpriteRenderer2D>();

            // Log render data once.
            if (!s_renderLogged)
            {
                s_renderLogged = true;
                System.Console.WriteLine(
                    "[BarnacleSnatcher] Render state: " +
                    $"animTexId={anim.TextureId} rendererTexId={renderer.TextureId} " +
                    $"frame={anim.FrameWidth}x{anim.FrameHeight} sheet={anim.SheetWidth}x{anim.SheetHeight} " +
                    $"row={anim.Row} offset={anim.FrameOffset} length={anim.FrameLength} useRow={anim.UseRow} " +
                    $"playing={anim.Playing} fps={anim.FramesPerSecond:0.##} " +
                    $"stateFrame={state.CurrentFrame} finished={state.Finished} " +
                    $"renderSize={renderer.Width}x{renderer.Height} " +
                    $"uvOffset={storedRenderer.Offset.X:0.###},{storedRenderer.Offset.Y:0.###} " +
                    $"uvTiling={storedRenderer.Tiling.X:0.###},{storedRenderer.Tiling.Y:0.###} " +
                    $"colorRGBA={storedRenderer.Color.R:0.##},{storedRenderer.Color.G:0.##}," +
                    $"{storedRenderer.Color.B:0.##},{storedRenderer.Color.A:0.##}");
            }

            // Log entity transform/active state once.
            if (!s_entityLogged)
            {
                s_entityLogged = true;
                var entity = result.Entity;
                string activeState = "missing";
                if (entity.TryGetComponent<Active>(out var active))
                {
                    activeState = active.Enabled ? "enabled" : "disabled";
                }

                string position = "missing";
                string scale = "missing";
                if (entity.TryGetComponent<LocalTransform>(out var local))
                {
                    position = $"{local.Position.X:0.###},{local.Position.Y:0.###},{local.Position.Z:0.###}";
                    scale = $"{local.Scale.X:0.###},{local.Scale.Y:0.###},{local.Scale.Z:0.###}";
                }

                System.Console.WriteLine(
                    "[BarnacleSnatcher] Entity state: " +
                    $"active={activeState} pos={position} scale={scale}");
            }

            // Optional verbose logs every half second or on changes.
            if (ai.DebugLogs)
            {
                ai.DebugLogTimer -= Time.DeltaTime;
                bool animChanged = ai.LastAnimRow != anim.Row ||
                                   ai.LastFrameOffset != anim.FrameOffset ||
                                   ai.LastFrameLength != anim.FrameLength;
                bool stateChanged = ai.LastState != ai.State;
                if (ai.DebugLogTimer <= 0.0f || animChanged || stateChanged)
                {
                    ai.DebugLogTimer = 0.5f;
                    ai.LastState = ai.State;
                    ai.LastAnimRow = anim.Row;
                    ai.LastFrameOffset = anim.FrameOffset;
                    ai.LastFrameLength = anim.FrameLength;
                    System.Console.WriteLine(
                        $"[BarnacleSnatcher] State={ai.State} InRange={ai.IsInRange} Cooldown={ai.AttackCooldownTimer:0.00} " +
                        $"Row={anim.Row} Offset={anim.FrameOffset} Length={anim.FrameLength} FPS={anim.FramesPerSecond:0.##} " +
                        $"Finished={state.Finished} CurrentFrame={state.CurrentFrame}");
                }
            }
        }

        // Report if no entities matched the query.
        if (barnacleCount == 0)
        {
            System.Console.WriteLine("[BarnacleSnatcher] No entities matched BarnacleSnatcherComponent + Animation + Renderer.");
        }
    }

    protected override void OnDestroy()
    {
        // Cleanup any brain handles.
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

    // Ensure a brain handle exists and is in patrol.
    private static Brain GetOrCreateBrain(ref BarnacleSnatcherComponent ai)
    {
        if (ai.BrainHandle == 0)
        {
            Brain created = Brain.Create();
            nint patrolState = created.GetPatrolState();
            if (patrolState != nint.Zero)
            {
                created.TransitionTo(patrolState);
            }
            ai.BrainHandle = (ulong)created.Handle;
            return created;
        }

        return Brain.FromHandle((nint)ai.BrainHandle);
    }

    // Transition to a new brain state if different.
    private static nint TransitionIfDifferent(Brain brain, nint currentState, nint targetState)
    {
        if (currentState == targetState)
        {
            return currentState;
        }

        brain.TransitionTo(targetState);
        return targetState;
    }

    // Apply attack animation settings.
    private static void SetAttack(ref BarnacleSnatcherComponent ai, ref SpriteSheetAnimation2D anim, ref AnimationState2D state)
    {
        //Log("SNATCH SNATCH SNATCH");
        //It will eat both for now
        InventoryController.instance.DecrementInStackSlot(1);
        InventoryController.instance.DecrementInStackSlot(2);

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

    // Apply idle animation settings.
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

    // Ensure idle animation matches desired config.
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

    // Ensure attack animation matches desired config.
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

    // Initialize defaults once.
    private static void EnsureDefaults(ref BarnacleSnatcherComponent ai)
    {
        if (ai.Initialized)
            return;

        if (ai.IdleFrameCount <= 0 && ai.AttackFrameCount <= 0)
        {
            ai.IdleStartFrame = 0;
            ai.IdleFrameCount = 20;
            ai.IdleFps = 30.0f;
            ai.AttackStartFrame = 1;
            ai.AttackFrameCount = 20;
            ai.AttackFps = 24.0f;
            ai.AttackLoop = false;
            ai.State = BarnacleState.Idle;
        }

        if (ai.AnimRow < 0)
            ai.AnimRow = 0;

        ai.AnimRow = 0;
        ai.IdleFrameOffset = 0;
        ai.IdleFrameLength = 1;
        ai.AttackFrameOffset = 1;
        ai.AttackFrameLength = 20;

        ai.Initialized = true;
    }

    // Find barnacle by name for other systems.
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

    // Name matcher used in this system.
    private static bool IsName(string actual, string expected)
        => actual.Length != 0 && string.Equals(actual, expected, StringComparison.OrdinalIgnoreCase);

    // Find squidward transform and collider by name.
    private static bool TryGetSquidwardData(World world, out LocalTransform transform, out BoxCollider2D collider)
    {
        foreach (var result in world.Query<Name, LocalTransform, BoxCollider2D>())
        {
            string name = Strings.Resolve(result.Component1.Value) ?? string.Empty;
            if (IsName(name, "squidward"))
            {
                transform = result.Component2;
                collider = result.Component3;
                return true;
            }
        }

        transform = default;
        collider = default;
        return false;
    }

    // Simple AABB overlap test.
    private static bool AabbOverlap(
        in LocalTransform aTransform,
        in BoxCollider2D aCollider,
        in LocalTransform bTransform,
        in BoxCollider2D bCollider)
    {
        float aScaleX = Math.Abs(aTransform.Scale.X);
        float aScaleY = Math.Abs(aTransform.Scale.Y);
        float bScaleX = Math.Abs(bTransform.Scale.X);
        float bScaleY = Math.Abs(bTransform.Scale.Y);

        float aCenterX = aTransform.Position.X + aCollider.Offset.X;
        float aCenterY = aTransform.Position.Y + aCollider.Offset.Y;
        float bCenterX = bTransform.Position.X + bCollider.Offset.X;
        float bCenterY = bTransform.Position.Y + bCollider.Offset.Y;

        float aHalfX = aCollider.HalfExtents.X * aScaleX;
        float aHalfY = aCollider.HalfExtents.Y * aScaleY;
        float bHalfX = bCollider.HalfExtents.X * bScaleX;
        float bHalfY = bCollider.HalfExtents.Y * bScaleY;

        return Math.Abs(aCenterX - bCenterX) <= (aHalfX + bHalfX) &&
               Math.Abs(aCenterY - bCenterY) <= (aHalfY + bHalfY);
    }

    // Debug logging for AABB overlap.
    private static void LogAabb(
        string aName,
        in LocalTransform aTransform,
        in BoxCollider2D aCollider,
        string bName,
        in LocalTransform bTransform,
        in BoxCollider2D bCollider,
        bool overlap)
    {
        float aCenterX = aTransform.Position.X + aCollider.Offset.X;
        float aCenterY = aTransform.Position.Y + aCollider.Offset.Y;
        float bCenterX = bTransform.Position.X + bCollider.Offset.X;
        float bCenterY = bTransform.Position.Y + bCollider.Offset.Y;

        float aHalfX = aCollider.HalfExtents.X * Math.Abs(aTransform.Scale.X);
        float aHalfY = aCollider.HalfExtents.Y * Math.Abs(aTransform.Scale.Y);
        float bHalfX = bCollider.HalfExtents.X * Math.Abs(bTransform.Scale.X);
        float bHalfY = bCollider.HalfExtents.Y * Math.Abs(bTransform.Scale.Y);

        System.Console.WriteLine(
            $"[BarnacleSnatcher] AABB overlap={overlap} " +
            $"{aName} center=({aCenterX:0.###},{aCenterY:0.###}) half=({aHalfX:0.###},{aHalfY:0.###}) " +
            $"{bName} center=({bCenterX:0.###},{bCenterY:0.###}) half=({bHalfX:0.###},{bHalfY:0.###})");
    }

    // Apply fixed sprite sheet metadata every update.
    private static void EnsureSheetMetadata(ref SpriteSheetAnimation2D anim, ref SpriteRenderer2D renderer)
    {
        if (!s_barnacleTexturePathId.IsValid)
        {
            s_barnacleTexturePathId = Strings.Intern(BarnacleTexturePath);
        }

        anim.TexturePath = s_barnacleTexturePathId;
        renderer.TexturePath = s_barnacleTexturePathId;

        anim.FrameWidth = BarnacleFrameWidth;
        anim.FrameHeight = BarnacleFrameHeight;
        anim.SheetWidth = BarnacleSheetWidth;
        anim.SheetHeight = BarnacleSheetHeight;
        anim.FrameCount = BarnacleFrameCount;
        anim.StartFrame = 0;
        if (anim.TextureId == 0 && renderer.TextureId != 0)
        {
            anim.TextureId = renderer.TextureId;
        }

        if (!s_metadataLogged)
        {
            s_metadataLogged = true;
            System.Console.WriteLine(
                "[BarnacleSnatcher] Applied sheet metadata from code: " +
                $"{BarnacleTexturePath} {BarnacleSheetWidth}x{BarnacleSheetHeight} " +
                $"frame {BarnacleFrameWidth}x{BarnacleFrameHeight} count={BarnacleFrameCount}");
        }
    }
}
