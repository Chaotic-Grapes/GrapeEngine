using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using System.Collections.Generic;


namespace EchoesBelow.Scripts.MarineSnowSystem;

[Component] public record struct MS_TestComponent(int returnThis, int pullThis, int child, int parent);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class MS_Test : SystemBase
{
    protected override void OnCreate()
    {
        Log("System MS_Test initialized", LogLevel.Debug);
    }
    //Purely to test out obj pool
    protected override void OnUpdate()
    {
        foreach ( var result in World!.Query<MS_TestComponent>())
        {
            if (Input.IsKeyPressed(KeyCode.P))
            {
                MS_Manager.instance.TakeFromPool(result.Component1.pullThis, new Vector3(0, 0, 0),5);
            }
            if (Input.IsKeyPressed(KeyCode.R))
            {
                MS_Manager.instance.SendToPool((ulong)result.Component1.returnThis);
            }

            if (Input.IsKeyPressed(KeyCode.I))
            {
                Entity child = Entity.FromId(World!,(ulong)result.Component1.child);
                Entity parent = Entity.FromId(World!, (ulong)result.Component1.parent);

                Log("parent . . . ");
                
                child.AttachTo(parent);
                //child.GetParent();
                Log("Attempted parenting: " + child.GetParent()!.GetComponent<Name>().ToString());
            }
            if (Input.IsKeyPressed(KeyCode.K))
            {
                Entity child = Entity.FromId(World!, (ulong)result.Component1.child);
                Entity parent = Entity.FromId(World!, (ulong)result.Component1.parent);
                Log("child . . . ");
                child.Detach();
                Log("I wanna live on my own!");
            }
        }

    }
}
