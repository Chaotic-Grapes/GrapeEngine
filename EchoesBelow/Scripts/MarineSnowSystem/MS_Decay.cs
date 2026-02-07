using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Collections.Generic;


namespace EchoesBelow.Scripts.MarineSnowSystem;
[Component] public record struct MS_DecayComponent(float decayTime);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class MS_Decay : SystemBase
{
    protected override void OnCreate()
    {
        //Log("System MS_Decay initialized");
    }

    protected override void OnUpdate()
    {
       foreach(var gameObject in World!.Query<MS_DecayComponent, MS_ManagerComponent>())
       {
            gameObject.Component1.decayTime -= Time.DeltaTime;
            if(gameObject.Component1.decayTime < 0)
            {
                //fasten your seatbelts!
                //I am the servant of the secret fire,
                //Wielder of the flame of Arnor!
                //The dark fire shall not avail you!
                //Flame of Udun!
                //AHH
                //Go back to the shadow!
                MS_Manager.instance.SendToPool(gameObject.Entity.Id);
                //Gandalf!
            }
       }
    }
 
}
