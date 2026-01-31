using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[Component] public record struct PlayerComponent(
    //[Pseudo-SerializeField]
    float moveSpeed,
    float Time,
    float lerpFac
);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Player : SystemBase
{
    Vector2 moveDir = Vector2.Zero;
    protected override void OnCreate()
    {
        Log("System Player initialized");
    }

    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<PlayerComponent, LinearVelocity2D, AngularVelocity2D>())
        {
            ref LinearVelocity2D lv = ref gameObject.Component2;
            float moveSpeed = gameObject.Component1.moveSpeed;

            if (Input.IsKeyDown(KeyCode.W) || Input.IsKeyDown(KeyCode.A) ||
                Input.IsKeyDown(KeyCode.S) || Input.IsKeyDown(KeyCode.D))
            {
                if (Input.IsKeyDown(KeyCode.W))
                {
                    lv.Value.Y += 0.01f * moveSpeed;
                    //moveDir.Y = 1;
                    Log("Up!");
                }
                if (Input.IsKeyDown(KeyCode.S))
                {
                    lv.Value.Y -= 0.01f * moveSpeed;
                    //moveDir.Y = -1;
                    Log("Down!");
                }
                if (Input.IsKeyDown(KeyCode.A))
                {
                    lv.Value.X -= 0.01f * moveSpeed;
                    //moveDir.X = -1;
                    Log("Left!");
                }
                if (Input.IsKeyDown(KeyCode.D))
                {
                    lv.Value.X += 0.01f * moveSpeed;
                    //moveDir.X = 1;
                    Log("Right!");
                }
            }
            else
            {
                float lerpFac = gameObject.Component1.lerpFac;
                //moveDir = new Vector2(GMath.Lerp(moveDir.X, 0, 0.01f * lerpFac), GMath.Lerp(moveDir.Y, 0, 0.01f * lerpFac));
                lv.Value = new Vector2(GMath.Lerp(lv.Value.X, 0, 0.01f * lerpFac), GMath.Lerp(lv.Value.Y, 0, 0.01f * lerpFac));
                Log("Resting!");
            }

            //Log("Pre Normalize");
            //lv.Value = moveDir.Normalized * 0.01f * moveSpeed;
            //Log("Normalized and returned");
        }
    }

    protected override void OnDestroy()
    {
        Log("System Player destroyed");
    }
}
