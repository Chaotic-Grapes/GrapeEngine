using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[Component] public record struct SpinnerComponent(float InputTest, float x, float y);
/// <summary>
/// System that processes entities with specific components.
/// This is a pure ECS system: it queries entities and updates their components.
/// </summary>
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Spinner : SystemBase
{
    protected override void OnCreate()
    {
        //Log("System Spinner initialized");
    }
    
    protected override void OnUpdate()
    {
        foreach(var result in World!.Query<SpinnerComponent, AngularVelocity2D, LinearVelocity2D>())
        {
            ref AngularVelocity2D av = ref result.Component2;
            av.Value = result.Component1.InputTest;

            ref LinearVelocity2D lv = ref result.Component3;
            lv.Value = new Vector2(result.Component1.x, result.Component1.y);

        }
    }

    protected override void OnDestroy()
    {
        //Log("System Spinner destroyed");
    }
}
