using EchoesBelow.Scripts;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Collections.Generic;

namespace Scripts.Menu;

[Component] public record struct PauseMenuComponent(bool isPauseable, int resumeSiginifier, int exitSignifier, bool start);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class PauseMenu : SystemBase
{
    private const string SourceSceneName = "Level_One";
    private const string TargetScenePath = "EchoesBelow/Scenes/M4StartScene.scn";
    bool isPaused = false;

    public static List<ulong> pauseMenuElementObjIds;
    protected override void OnCreate()
    {
        Log("System PauseMenu initialized");
    }
    private bool OnStart(ref bool startBool)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        pauseMenuElementObjIds = new List<ulong>();

        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        foreach(var pauseController in World!.Query<PauseMenuComponent>())
        {
            bool start = pauseController.Component1.start;
            pauseController.Component1.start = OnStart(ref start);

            if (!pauseController.Component1.isPauseable) return;
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
[Component] public record struct AddToPauseMenuListComponent(bool start);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class AddToPauseMenuList : SystemBase
{
    private bool OnStart(ref bool startBool, ulong pauseMenuElementId)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        PauseMenu.pauseMenuElementObjIds.Add(pauseMenuElementId);

        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        foreach(var pauseMenuElement in World!.Query<AddToPauseMenuListComponent>())
        {
            bool start = pauseMenuElement.Component1.start;
            pauseMenuElement.Component1.start = OnStart(ref start, pauseMenuElement.Entity.Id);
        }
    }
}
