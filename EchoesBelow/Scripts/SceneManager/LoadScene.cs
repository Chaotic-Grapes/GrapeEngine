using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;


[Component] public record struct LoadSceneComponent(float timeAccumulator);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class LoadScene : SystemBase
{
    protected override void OnCreate()
    {
        Log("System LoadScene initialized");
    }

    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<LoadSceneComponent>())
        {
            SceneManager sc = SceneManager.Instance;

            //Log($"SceneCount: {sc.GetSceneCount()} / CurrentSceneIndex: {sc.GetActiveIndex()} // Current Active: {sc.GetActive()!.Name}");
            //gameObject.Component1.timeAccumulator += Time.DeltaTime;
            //if (gameObject.Component1.timeAccumulator > 10)
            //{
            //    gameObject.Component1.timeAccumulator = 0;
            //    sc.LoadScene(sc.GetActiveIndex(),"M4");
            //    sc.LoadScene(sc.GetActiveIndex(), "M4.scn");
            //    sc.LoadScene(sc.GetActiveIndex(), "UntitledScene.scn");
            //    sc.LoadScene(sc.GetActiveIndex(), "Untitiled Scene.scn");
            //    Log("Reload!");
            //}
            Log("Hello");
        }
    }

    protected override void OnDestroy()
    {
        Log("System LoadScene destroyed");
    }
}
