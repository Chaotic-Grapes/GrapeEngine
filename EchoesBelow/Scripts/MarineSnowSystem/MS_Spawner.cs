using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using System.Collections.Generic;


namespace EchoesBelow.Scripts.MarineSnowSystem;

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class MS_Spawner : SystemBase
{
    protected override void OnCreate()
    {
        Log("System MS_Spawner initialized");
    }

    protected override void OnUpdate()
    {
        
    }
}
