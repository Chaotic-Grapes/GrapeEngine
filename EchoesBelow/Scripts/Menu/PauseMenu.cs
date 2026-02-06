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
    private const string SourceSceneName = "Level_One";
    private const string TargetScenePath = "EchoesBelow/Scenes/M4StartScene.scn";
    bool isPaused = false;
    protected override void OnCreate()
    {
        Log("System PauseMenu initialized");
    }

    protected override void OnUpdate()
    {
        SceneManager sceneManager = SceneManager.Instance;
        Scene? active = sceneManager.GetActive();
        if (active == null || !string.Equals(active.Name, SourceSceneName, StringComparison.Ordinal))
        {
            Log($"PauseMenu: Active scene is not the source scene: '{active?.Name ?? "null"}'; aborting switch.");
            return;
        }

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
