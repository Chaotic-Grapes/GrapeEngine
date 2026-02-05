using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[Component] public record struct DoorComponent(float offset, bool isVertical, bool isOpen);
/// <summary>
/// System that processes entities with specific components.
/// This is a pure ECS system: it queries entities and updates their components.
/// </summary>
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Door : SystemBase
{
    protected override void OnCreate()
    {
       
    }
    
    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<DoorComponent>())
        {
            ////skip if its not open
            //if (!gameObject.Component1.isOpen) continue;

            //ref LinearVelocity2D transform = ref gameObject.Entity.GetComponent<LocalTransform>();
            //if (gameObject.Component1.isVertical)
            //{


            //}
            //else
            //{

            //} //tagmask2 == 4
            Log("mask detected: " + gameObject.Entity.GetComponent<TagMask>().Mask);
        }
    }

}
