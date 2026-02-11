using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System;

namespace EchoesBelow.Scripts;

[Component] public record struct OscillatorComponent(bool start, float period, float timer, float height);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Oscillator : SystemBase
{
    float oscillateFac;
    //[SerializeField] float period = 1f;
    const float tau = GMath.Pi * 2f; //tau intitalized as 6.283
    Vector3 startPos = new Vector3(6.68f,2.86f,0);
    


    private bool OnStart(ref bool startBool, Vector3 pos)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        //startPos = pos;

        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {

        foreach(var result in World!.Query<OscillatorComponent, LocalTransform>())
        {
            //Log("StartPos1: " + startPos);
            bool start = result.Component1.start;
            result.Component1.start = OnStart(ref start, result.Component2.Position);
            if (!result.Component1.start) return;
            result.Component1.timer += Time.DeltaTime;

            if (result.Component1.period < GMath.Epsilon) return; //NaN protection Mathf.Epsilon is the smallest possible float in unity
            float cycles = result.Component1.timer / result.Component1.period; //determines the number of cycles passed
            float rawSineWave = GMath.Sin(cycles * tau); //creates my sine wave w the period indicated. Returns a value btwn -1 to 1
            oscillateFac = (rawSineWave + 1) / 2f; //converts the range (-1 to 1) to (0 to 2) then (0-1)


            result.Entity.GetComponent<LocalTransform>().Position
            = new Vector3(result.Entity.GetComponent<LocalTransform>().Position.X,
                          startPos.Y + (result.Component1.height * oscillateFac),0);
            //Log("StartPos2: " + startPos);
        }
    }

    protected override void OnDestroy()
    {
        //Log("System Spinner destroyed");
    }
}



