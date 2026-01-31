using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[Component] public record struct CompCheckerComponent(float Time);
/// <summary>
/// System that processes entities with specific components.
/// This is a pure ECS system: it queries entities and updates their components.
/// </summary>
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class CompChecker : SystemBase
{
    protected override void OnCreate()
    {
        Log("System CompChecker initialized");
    }

    protected override void OnUpdate()
    {
        //If an obj in the scene has Compchecker, this whole script is void!
        //Just a way to turn it on or off
        foreach (var result in World!.Query<CompCheckerComponent>())
        {
            return;
        }


        int i = 0;
        Log("Checking. . . ");
        Log("________________________________________________________________________________", LogLevel.Debug); i++;
        foreach (var result in World.Query<Name>())
        {
            Log($"{i}) Found Name belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Value = Strings.Intern("POLYP");
            Log($"Done: {Strings.Resolve(storedComponent.Value)}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<TagMask>())
        {
            Log($"{i}) Found TagMask belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Mask = 1;
            Log($"Done: {storedComponent.Mask}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<Active>())
        {
            Log($"{i}) Found Active belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Enabled = !storedComponent.Enabled;
            Log("Done// Set to Toggle, watch for Changes");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<Layer>())
        {
            Log($"{i}) Found Layer belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Id = 2;
            Log($"Done: {storedComponent.Id}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<LocalTransform>())
        {
            Log($"{i}) Found LocalTransform belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Rotation = new Quaternion(3, 4, 5, 1);
            storedComponent.Position = new Vector3(6, 7, 8);
            storedComponent.Scale = new Vector3(9, 10, 11);
            Log($"Done: {storedComponent.Rotation} , {storedComponent.Position} , {storedComponent.Scale}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<WorldTransform>())
        {
            Log($"{i}) Found WorldTransform belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;

            Log("Done, did not touch!");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;


        foreach (var result in World.Query<LinearVelocity2D>())
        {
            Log($"{i}) Found LinearVelocity2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Value = new GrapeEngine.Math.Vector2(0, 13);
            Log($"Done: {storedComponent.Value}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<Acceleration2D>())
        {
            Log($"{i}) Found Acceleration2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Value = new GrapeEngine.Math.Vector2(14, 15);

            Log($"Done: {storedComponent.Value}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<AngularVelocity2D>())
        {
            Log($"{i}) Found AngularVelocity2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Value = 16;
            Log($"Done: {storedComponent.Value}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;



        foreach (var result in World.Query<Rigidbody2D>())
        {
            Log($"{i}) Found Rigidbody2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Mass = 17;
            //storedComponent.InverseMass = 18;
            storedComponent.LinearDamping = 19;
            storedComponent.AngularDamping = 20;
            storedComponent.GravityScale = 21;
            storedComponent.Flags = 22;
            Log($"Done: {storedComponent.Mass} , {storedComponent.InverseMass} , {storedComponent.LinearDamping} , {storedComponent.AngularDamping} , {storedComponent.GravityScale} , {storedComponent.Flags}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;


        foreach (var result in World.Query<PhysicsMaterial2D>())
        {
            Log($"{i}) Found PhysicsMaterial2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Friction = 23;
            storedComponent.Restitution = 24;
            storedComponent.PositionCorrectPercent = 25;
            Log($"Done: {storedComponent.Friction} , {storedComponent.Restitution} , {storedComponent.PositionCorrectPercent}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<BoxCollider2D>())
        {
            Log($"{i}) Found BoxCollider2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.IsTrigger = !storedComponent.IsTrigger;
            storedComponent.Offset = new Vector2(26, 27);
            storedComponent.HalfExtents = new Vector2(28, 29);
            storedComponent.Rotation = 30;
            storedComponent.LayerMask = 31;
            Log($"Done: {storedComponent.IsTrigger} , {storedComponent.Offset} , {storedComponent.HalfExtents} , {storedComponent.Rotation} , {storedComponent.LayerMask}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<CircleCollider2D>())
        {
            Log($"{i}) Found CircleCollider2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.IsTrigger = !storedComponent.IsTrigger;
            storedComponent.Offset = new Vector2(32, 33);
            storedComponent.Radius = 34;
            storedComponent.LayerMask = 35;
            Log($"Done: {storedComponent.IsTrigger} , {storedComponent.Offset} , {storedComponent.Radius} , {storedComponent.LayerMask}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<SpriteRenderer2D>())
        {
            Log($"{i}) Found SpriteRenderer2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Color = Color.Blue;
            storedComponent.Tiling = new Vector2(36, 37);
            storedComponent.Offset = new Vector2(38, 39);
            Log($"Done: {storedComponent.Color} , {storedComponent.Tiling} , {storedComponent.Offset}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<SpriteSheetAnimation2D>())
        {
            Log($"{i}) Found SpriteSheetAnimation2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.FrameWidth = 40;
            storedComponent.FrameHeight = 41;
            storedComponent.SheetWidth = 42;
            storedComponent.SheetHeight = 43;
            storedComponent.StartFrame = 44;
            storedComponent.FrameCount = 45;
            storedComponent.FramesPerSecond = 46;
            storedComponent.Loop = !storedComponent.Loop;
            storedComponent.Playing = !storedComponent.Playing;
            Log($"Done: {storedComponent.FrameWidth} , {storedComponent.FrameHeight} , {storedComponent.SheetWidth}");
            Log($"Done: {storedComponent.SheetHeight} , {storedComponent.StartFrame} , {storedComponent.FrameCount}");
            Log($"Done: {storedComponent.FramesPerSecond} , {storedComponent.Loop} , {storedComponent.Playing}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<AnimationState2D>())
        {
            Log($"{i}) Found AnimationState2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.CurrentFrame = 47;
            storedComponent.TimeAccumulator = 48;
            storedComponent.Finished = !storedComponent.Finished;
            Log($"Done: {storedComponent.CurrentFrame} , {storedComponent.TimeAccumulator} , {storedComponent.Finished}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<ShapeCircle2D>())
        {
            Log($"{i}) Found ShapeCircle2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Radius = 49;
            storedComponent.Offset = new Vector2(50, 51);
            storedComponent.Color = Color.Red;
            storedComponent.Thickness = 52;
            storedComponent.Filled = !storedComponent.Filled;
            Log($"Done: {storedComponent.Radius} , {storedComponent.Offset} , {storedComponent.Color} , {storedComponent.Thickness} , {storedComponent.Filled}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<ShapeBox2D>())
        {
            Log($"{i}) Found ShapeBox2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.HalfExtents = new Vector2(53, 54);
            storedComponent.Offset = new Vector2(55, 56);
            storedComponent.Color = Color.Yellow;
            storedComponent.Thickness = 58;
            storedComponent.Filled = !storedComponent.Filled;
            Log($"Done: {storedComponent.HalfExtents} , {storedComponent.Offset} , {storedComponent.Color} , {storedComponent.Thickness} , {storedComponent.Filled}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<ShapeLine2D>())
        {
            Log($"{i}) Found ShapeLine2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.A = new Vector2(59, 60);
            storedComponent.B = new Vector2(61, 62);
            storedComponent.Thickness = 63;
            storedComponent.Color = Color.Cyan;
            Log($"Done: {storedComponent.A} , {storedComponent.B} , {storedComponent.Thickness} , {storedComponent.Color}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<ZIndex2D>())
        {
            Log($"{i}) Found Z-Index2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.ZOrder = 64;
            Log($"Done: {storedComponent.ZOrder}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<Camera3D>())
        {
            Log($"{i}) Found Camera3D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Active = !storedComponent.Active;
            storedComponent.UsePerspective = !storedComponent.UsePerspective;
            storedComponent.OrthoSize = 65;
            storedComponent.NearPlane = 66;
            storedComponent.FarPlane = 67;
            storedComponent.AspectRatio = 1.23f;
            Log($"Done: {storedComponent.Active} , {storedComponent.UsePerspective} , {storedComponent.OrthoSize} , {storedComponent.NearPlane} , {storedComponent.FarPlane} , {storedComponent.AspectRatio}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<Light2D>())
        {
            Log($"{i}) Found Light2D belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.LightType = Light2D.Type.Point; // this means 1
            storedComponent.Position = new Vector3(68, 69, 70);
            storedComponent.Direction = new Vector3(71, 72, 73);
            storedComponent.Color = Color.Green;
            storedComponent.Intensity = 74;
            storedComponent.Range = 75;
            storedComponent.CastsShadows = !storedComponent.CastsShadows;
            Log($"Done: {storedComponent.LightType} , {storedComponent.Position} , {storedComponent.Direction} , " +
                $"{storedComponent.Color} , {storedComponent.Intensity} , {storedComponent.Range} , {storedComponent.CastsShadows}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        foreach (var result in World.Query<AudioSource>())
        {
            Log($"{i}) Found AudioSource belonging to ID: {result.Entity.Id}");
            ref var storedComponent = ref result.Component1;
            storedComponent.Volume = 76;
            storedComponent.Pitch = 77;
            storedComponent.Loop = !storedComponent.Loop;
            storedComponent.PlayOnStart = !storedComponent.PlayOnStart;
            storedComponent.Spatial3D = !storedComponent.Spatial3D;
            Log($"Done: {storedComponent.Volume} , {storedComponent.Pitch} , {storedComponent.Loop} , {storedComponent.PlayOnStart} , {storedComponent.Spatial3D}");
        }
        Log("________________________________________________________________________________", LogLevel.Debug); i++;

        Log("End of Finding");
    }

    protected override void OnDestroy()
    {
        Log("System CompChecker destroyed");
    }
}
