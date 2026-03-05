using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace Scripts;

/// <summary>
/// System that processes entities with specific components.
/// This is a pure ECS system: it queries entities and updates their components.
/// </summary>
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
[RequireForUpdate<GUIText>()] // This means that the system will only run if there is at least one entity with a GUIText component
public class RequireForUpdateTestSystem : SystemBase
{
    private bool _test = false;

    protected override void OnCreate()
    {
        Log("System RequireForUpdateTestSystem initialized");
    }

    protected override void OnStartRunning()
    {
        Log("System RequireForUpdateTestSystem started running");
    }

    protected override void OnSceneStart()
    {
        _test = true;
        Log($"_test: {_test}");
    }

    protected override void OnUpdate()
    {
        Log("OnUpdate called");
    }

    protected override void OnStopRunning()
    {
        Log("System RequireForUpdateTestSystem stopped running");
    }

    protected override void OnSceneStop()
    {
        _test = false;
        Log($"_test: {_test}");
    }

    protected override void OnDestroy()
    {
        Log("System RequireForUpdateTestSystem destroyed");
    }
}
