using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using System.Collections.Generic;


namespace EchoesBelow.Scripts;

//[Component] public record struct PlayerAnimManagerComponent(bool start);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class PlayerAnimManager : SystemBase
{
    public static PlayerAnimManager instance;
    protected override void OnCreate()
    {
        instance = this;
        //Log("System PlayerAnimManager initialized", LogLevel.Debug);
    }
    //private bool OnStart(ref bool startBool, ulong objId)
    //{
    //    if (startBool == true) return true;
    //    startBool = true;
    //    //Todo
        
    //    //End of Start
    //    return true;
    //}
    //protected override void OnUpdate()
    //{

    //    foreach(var gameObject in World!.Query<PlayerAnimManagerComponent>())
    //    {
    //        bool start = gameObject.Component1.start;
    //        gameObject.Component1.start = OnStart(ref start, gameObject.Entity.Id);
    //    }


    //}
    public void SetAnimState(PlayerAnimPreset animState)
    {
        foreach(var animator in  World!.Query<PlayerComponent, SpriteSheetAnimation2D>())
        {
            animator.Component2.Row = animState.row;
            animator.Component2.FrameOffset = animState.frameOffset;
            animator.Component2.FrameLength = animState.frameLength;
            animator.Component2.FramesPerSecond = animState.fps;
        }
    }
}
