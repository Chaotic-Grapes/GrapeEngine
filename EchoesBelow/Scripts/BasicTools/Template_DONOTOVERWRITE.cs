using EchoesBelow.Scripts.Audio;
using EchoesBelow.Scripts.MarineSnowSystem;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts.BasicTools;

//[Component] public record struct Template_DONOTOVERWRITEComponent(bool start);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Template_DONOTOVERWRITE : SystemBase
{
    protected override void OnCreate()
    {
        //Log("System Template_DONOTOVERWRITE initialized");
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
        //Use this
        //foreach(var gameObject in World!.Query<Template_DONOTOVERWRITEComponent>())
        //{
        //    bool start = gameObject.Component1.start;
        //    gameObject.Component1.start = OnStart(ref start);

        //    //Do everyth else
            

        //}
    }
}
