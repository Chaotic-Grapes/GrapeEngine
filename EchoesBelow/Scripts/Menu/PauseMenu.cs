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

    public bool isKeyPressed_vertical;
    bool isFirstSelected;

    Color selectedCol = new Color(0.370f, 0.376f, 0.584f,1f);
    Color unselectedCol = new Color(0.071f, 0.078f, 0.305f,1f);

    public static List<ulong> pauseMenuElementObjIds = new List<ulong>();
    protected override void OnCreate()
    {
        Log("System PauseMenu initialized");
    }
    private bool OnStart(ref bool startBool)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        isFirstSelected = true;
        //End of Start
        return true;
    }
    //Pause Menu is off by default
    protected override void OnUpdate()
    {
        foreach (var pauseController in World!.Query<PauseMenuComponent>())
        {
            bool start = pauseController.Component1.start;
            pauseController.Component1.start = OnStart(ref start);

            if (!pauseController.Component1.isPauseable) return;
        }
        //Test
        //foreach (ulong ui_id in pauseMenuElementObjIds)
        //{
        //    Entity.FromId(World!, ui_id).GetComponent<GUIElement>().Visible = false;
        //}
        SceneManager sceneManager = SceneManager.Instance;

        if (!isPaused && Input.IsKeyPressed(KeyCode.Escape))
        {
            Time.TimeScale = 0;
            isPaused = true;
            foreach (ulong ui_id in pauseMenuElementObjIds)
            {
                Entity.FromId(World!, ui_id).GetComponent<GUIElement>().Visible = true;
            }
            //Launch Pause Menu
            Log(() => "Paused");
        }
        else if(isPaused && Input.IsKeyPressed(KeyCode.Escape))
        {
            Time.TimeScale = 1;
            isPaused = false;
            foreach (ulong ui_id in pauseMenuElementObjIds)
            {
                Entity.FromId(World!, ui_id).GetComponent<GUIElement>().Visible = false;
            }
            //Close Pause Menu
            Log(() => "unPaused");
        }

        if (isPaused)
        {
            isKeyPressed_vertical = Input.IsKeyPressed(KeyCode.W) || Input.IsKeyPressed(KeyCode.A) || Input.IsKeyPressed(KeyCode.S) || Input.IsKeyPressed(KeyCode.D);
            if (isKeyPressed_vertical)
            {
                isFirstSelected = !isFirstSelected;
                //Log("isLeftSelected: " + isFirstSelected);
                foreach (var controller in World!.Query<PauseMenuComponent>())
                {
                    foreach (var ui in World!.Query<GUIElement, MatchSignifierComponent>())
                    {
                        if (ui.Component2.signifierID == controller.Component1.resumeSiginifier || ui.Component2.signifierID == controller.Component1.exitSignifier)
                        {
                            //ui.Entity.GetComponent<GUIElement>().Visible = !ui.Entity.GetComponent<GUIElement>().Visible;
                            ref GUIPanel panel = ref ui.Entity.GetComponent<GUIPanel>();
                            if (panel.Color.R == unselectedCol.R)
                            {
                                panel.Color = selectedCol;
                            }
                            else if(panel.Color.R == selectedCol.R)
                            {
                                panel.Color = unselectedCol;
                            }
                        }
                    }
                }
            }
        }

        if (isFirstSelected && Input.IsKeyPressed(KeyCode.Space))
        {
            Time.TimeScale = 1;
            isPaused = false;
            foreach (ulong ui_id in pauseMenuElementObjIds)
            {
                Entity.FromId(World!, ui_id).GetComponent<GUIElement>().Visible = false;
            }
            //Close Pause Menu
            Log(() => "unPaused");

        }
        else if (!isFirstSelected && Input.IsKeyPressed(KeyCode.Space))
        {
            Log("QUitting . . . ");
            sceneManager.SetNextAudioTransition(2.0f, true);
            //var scene = SceneManager.Instance.LoadScene(TargetScenePath);
            //Like creating a new scene / allocate a new scene in the registry
            Time.TimeScale = 1;
            isPaused = false;
            foreach (ulong ui_id in pauseMenuElementObjIds)
            {
                Entity.FromId(World!, ui_id).GetComponent<GUIElement>().Visible = false;
            }
            var sceneIndex = SceneManager.Instance.AddScene();
            var ss = SceneManager.Instance.LoadScene(sceneIndex, TargetScenePath);
            SceneManager.Instance.SetActive(sceneIndex);
           Log("Quit to screen");
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
