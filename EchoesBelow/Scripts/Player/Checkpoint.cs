using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using System.Collections.Generic;


namespace EchoesBelow.Scripts;

[Component] public record struct CheckPointComponent(bool start);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Checkpoint : SystemBase
{
    bool isHit = false;
    public static Checkpoint instance;
    public static Vector2 checkPointPos;
    protected override void OnCreate()
    {
        instance = this;
        Log("System Checkpoint initialized", LogLevel.Debug);
    }
    private bool OnStart(ref bool startBool, ulong objId)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        LocalTransform transform = Entity.FromId(World!, objId).GetComponent<LocalTransform>();
        checkPointPos = new Vector2(transform.Position.X, transform.Position.Y);
        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {

        foreach(var gameObject in World!.Query<CheckPointComponent>())
        {
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start, gameObject.Entity.Id);

        }


    }
}
