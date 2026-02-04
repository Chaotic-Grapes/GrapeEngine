using EchoesBelow.Scripts.MarineSnowSystem;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[Component] public record struct FollowLeaderComponent(int target_signifierID);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class FollowLeader : SystemBase
{
    protected override void OnCreate()
    {
        Log("System FollowLeader initialized");
    }

    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<FollowLeaderComponent, LocalTransform>())
        {
            //This is how we find matching signifiers============================================================

            ulong targetObjId =MS_Manager.instance.emptyId; //default objId, just borrowing MS_Manager's empty ID

            foreach(var result in World!.Query<MatchSignifierComponent>())
            {
                if(result.Component1.signifierID == gameObject.Component1.target_signifierID)
                {
                    targetObjId = result.Entity.Id;
                }
            }

            //===================================================================================================
            Entity entity = Entity.FromId(World!, gameObject.Entity.Id);
            ref LocalTransform transform = ref gameObject.Component2;

            Entity targetEntity = Entity.FromId(World!, targetObjId);
            ref LocalTransform targetTransform = ref targetEntity.GetComponent<LocalTransform>();

            transform.Position = new Vector3(GMath.Lerp(transform.Position.X, targetTransform.Position.X, 1f),
                                             GMath.Lerp(transform.Position.Y, targetTransform.Position.Y, 1f), 0f);
        }
    }

}
