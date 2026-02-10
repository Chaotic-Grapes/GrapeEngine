using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts.Audio;

[Component] public record struct AudioManagerComponent();
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class AudioManager : SystemBase
{
    protected override void OnCreate()
    {
        Log("System AudioManager initialized");
    }
    private bool OnStart(ref bool startBool)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo



        //End of Start
        return true;
    }

    protected override void OnUpdate()
    {
    
    }

    protected override void OnDestroy()
    {
        Log("System AudioManager destroyed");
    }
}
