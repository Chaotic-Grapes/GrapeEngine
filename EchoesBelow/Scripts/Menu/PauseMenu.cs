using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using System.Collections.Generic;

namespace Scripts.Menu;


[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class PauseMenu : SystemBase
{
    bool isPaused = false;
    protected override void OnCreate()
    {
        Log("System PauseMenu initialized");
    }

    protected override void OnUpdate()
    {
        if (!isPaused && Input.IsKeyPressed(KeyCode.Escape))
        {
            Time.TimeScale = 0;
            isPaused = true;
            //Launch Pause Menu
            Log(() => "Paused");
        }
        else if(isPaused && Input.IsKeyPressed(KeyCode.Escape))
        {
            Time.TimeScale = 1;
            isPaused = false;
            //Close Pause Menu
            Log(() => "unPaused");
        }
    }

}
