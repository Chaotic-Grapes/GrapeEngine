using GrapeEngine.Scripting.Core;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Events;

namespace EchoesBelow.Scripts.Experimental;

[Component] public record struct CollisionTestComponent;
//[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
[System(SystemGroup.PostPhysics, SystemRunMode.PlayOnly)]
public class CollisionTest : CollisionSystemBase
{
    //protected override void OnCreate()
    //{
    //    Log("System CollisionTest initialized");
    //}

    //protected override void OnCollisionEnter(Entity self, CollisionEvent evt)
    //{
    //    base.OnCollisionEnter(self, evt);
    //    Log("CollisionEntered");
    //    //Log("evt contact: " + evt.ContactPoint);
    //    //Log("self: " + self);
    //    ////foreach(var gameObject in World!.Query<CollisionTestComponent>())
    //    ////{
    //    ////    Log("Hello I Collided");
    //    ////}
    //}
    //protected override void OnCollisionExit(Entity self, CollisionExitEvent evt)
    //{
    //    base.OnCollisionExit(self, evt);
    //    Log("CollisionExit");
    //    //foreach (var gameObject in World!.Query<CollisionTestComponent>())
    //    //{
    //    //    Log("Hello I Exited");
    //    //}
    //}

    //protected override void OnDestroy()
    //{
    //    Log("System CollisionTest destroyed");
    //}
}
