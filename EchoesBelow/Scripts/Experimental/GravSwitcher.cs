using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[Component]public record struct GravSwitcherComponent(
    float TimeAccumulator, // must start below 0
    float switchInterval,
    float startPolarity
);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class GravSwitcher : SystemBase
{
    protected override void OnCreate()
    {
        Log("System GravSwitcher initialized");
    }

    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<GravSwitcherComponent, Rigidbody2D>())
        {
            //GetComponent
            ref Rigidbody2D rb = ref gameObject.Component2;


            if(gameObject.Component1.TimeAccumulator < 0)
            {
                //Fire at Start and whenever we reverse polarity
                rb.GravityScale = gameObject.Component1.startPolarity;

            }
            //timer
            gameObject.Component1.TimeAccumulator += Time.DeltaTime;

            if(gameObject.Component1.TimeAccumulator > gameObject.Component1.switchInterval)
            {
                gameObject.Entity.GetComponent<LinearVelocity2D>().Value = new Vector2(GMath.Random(-6,6), GMath.Random(-6,6));
                gameObject.Component1.TimeAccumulator = 0;
                rb.GravityScale *= -1;
            }
        }
    }

    protected override void OnDestroy()
    {
        Log("System GravSwitcher destroyed");
    }
}
