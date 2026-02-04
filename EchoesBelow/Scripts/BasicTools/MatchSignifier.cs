using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;
//Signifier IDs are 10 digit numbers that can be used as unique signifiers
//Use case: I want to reference an obj using pseudo serialize field. 
//I would attach a signifier to that obj, input a signifierID
//then in my pseudo serialize, I will indicate that ID in World!.Query foreach loop

//a signifier ID looks like this : 1000000000, it must start with a 1
[Component] public record struct MatchSignifierComponent(int signifierID);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class MatchSignifier : SystemBase
{
    protected override void OnCreate()
    {
        Log("System FollowLeader initialized");
    }
}
