using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace Scripts.BasicTools;

/// <summary>
/// System that processes entities with specific components.
/// This is a pure ECS system: it queries entities and updates their components.
/// </summary>
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Template_DONOTOVERWRITE : SystemBase
{
    protected override void OnCreate()
    {
        Log("System Template_DONOTOVERWRITE initialized");
    }

    protected override void OnUpdate()
    {
        
    }

    protected override void OnDestroy()
    {
        Log("System Template_DONOTOVERWRITE destroyed");
    }
}
