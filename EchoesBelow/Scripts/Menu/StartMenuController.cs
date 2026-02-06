using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System;

namespace EchoesBelow.Scripts;

[Component] public record struct StartMenuControllerComponent(bool start, int startSignifier, int exitSignifier, bool isEndScene, float timer);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class StartMenuController : SystemBase
{
    //For startscene
    private const string SourceSceneName = "M4StartScene";
    private const string TargetScenePath = "EchoesBelow/Scenes/Level_One.scn";
    private const float SwitchDelaySeconds = 5.0f;

    //for endscene
    private const string EndSceneName = "EndScene";
    private const string StartSceneName = "EchoesBelow/Scenes/M4StartScene.scn";

    private float _elapsed;
    private bool _switched;


    public bool isKeyPressed_horizontal;
    bool isLeftSelected;
    protected override void OnCreate()
    { //Application.Shutdown
        //Application.Quit();
    }
    private bool OnStart(ref bool startBool, ulong endSceneControllerId)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        //Default
        isLeftSelected = true;

        //reset endscene timer every time
        Entity.FromId(World!,endSceneControllerId).GetComponent<StartMenuControllerComponent>().timer = 1f;
        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        SceneManager sceneManager = SceneManager.Instance;
        foreach(var controller in World!.Query<StartMenuControllerComponent>())
        {
            if (controller.Component1.isEndScene)
            {
                controller.Component1.timer -= Time.DeltaTime;
                if(Input.IsKeyDown(KeyCode.Space)) //controller.Component1.timer < 0 &&
                {
                    //Fade out music
                    sceneManager.SetNextAudioTransition(2.0f, true);
                    //var scene = SceneManager.Instance.LoadScene(TargetScenePath);
                    //Like creating a new scene / allocate a new scene in the registry
                    var sceneIndex = SceneManager.Instance.AddScene();
                    var ss = SceneManager.Instance.LoadScene(sceneIndex, StartSceneName);
                    SceneManager.Instance.SetActive(sceneIndex);
                }
            }
            else continue;
        }

        
        Scene? active = sceneManager.GetActive();
        if (active == null || !string.Equals(active.Name, SourceSceneName, StringComparison.Ordinal))
        {
            //Log($"StartMenuController: Active scene is not the source scene: '{active?.Name ?? "null"}'; aborting switch.");
            return;
        }


        foreach (var controller in World!.Query<StartMenuControllerComponent>())
        {
            bool start = controller.Component1.start;
            controller.Component1.start = OnStart(ref start, controller.Entity.Id);
        }

        isKeyPressed_horizontal = Input.IsKeyPressed(KeyCode.A) || Input.IsKeyPressed(KeyCode.D);
        if (isKeyPressed_horizontal)
        {
            isLeftSelected = !isLeftSelected;
            Log("isLeftSelected: " + isLeftSelected);
            foreach(var controller in World!.Query<StartMenuControllerComponent>())
            {
                foreach(var ui in World!.Query<GUIElement, MatchSignifierComponent>())
                {
                    if (ui.Component2.signifierID == controller.Component1.startSignifier || ui.Component2.signifierID == controller.Component1.exitSignifier)
                    {
                        ui.Entity.GetComponent<GUIElement>().Visible = !ui.Entity.GetComponent<GUIElement>().Visible;
                    }
                }
            }
        }

        if (isLeftSelected && Input.IsKeyPressed(KeyCode.Space))
        {
            //Load Scene
            //Log("Start");
            //SceneManager.Instance.LoadScene(TargetScenePath);
            //Log("Loaded");
            try
            {
                //Fade out music
                sceneManager.SetNextAudioTransition(2.0f, true);
                //var scene = SceneManager.Instance.LoadScene(TargetScenePath);
                //Like creating a new scene / allocate a new scene in the registry
                var sceneIndex = SceneManager.Instance.AddScene();
                var ss = SceneManager.Instance.LoadScene(sceneIndex, TargetScenePath);
                SceneManager.Instance.SetActive(sceneIndex);
                Log("Loaded");
            }
            catch (Exception ex)
            {
                Log($"{ex.Message}");
            }
        }
        else if(!isLeftSelected && Input.IsKeyPressed(KeyCode.Space))
        {
            Log("Quit");
            Application.Quit();
        }
    }
}
