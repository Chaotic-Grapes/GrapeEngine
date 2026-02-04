using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[Component] public record struct CamFollowComponent();
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class CamFollow : SystemBase
{
    protected override void OnCreate()
    {
        Log("System CamFollow initialized");
    }

    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<CamFollowComponent, LocalTransform>())
        {
            Entity entity = Entity.FromId(World!, gameObject.Entity.Id);
            ref LocalTransform transform = ref gameObject.Component2;

            transform.Position = new Vector3(GMath.Lerp(transform.Position.X, Player.instance.currentPos.X, 0.1f),
                                             GMath.Lerp(transform.Position.Y, Player.instance.currentPos.Y, 0.1f), transform.Position.Z);
        }
    }

}
