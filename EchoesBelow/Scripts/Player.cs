using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[Component] public record struct PlayerComponent(
    //[Pseudo-SerializeField]
    float moveSpeed,
    float time,
    float lerpFac,
    float maxSpeed,
    float angularVelocity
    
);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Player : SystemBase
{
    protected override void OnCreate()
    {
        Log("System Player initialized");
    }

    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<PlayerComponent, LinearVelocity2D, AngularVelocity2D, LocalTransform>())
        {
            ref LocalTransform transform = ref gameObject.Component4;
            ref LinearVelocity2D lv = ref gameObject.Component2;
            Vector2 playerDir;
            Vector2 moveDir = Vector2.Zero;
            Vector2 moveDirNormalized = Vector2.Zero;
            float lerpFac = gameObject.Component1.lerpFac * 0.01f; //this allows floating point decimal values
            float moveSpeed = gameObject.Component1.moveSpeed * 0.01f; //this allows floating point decimal values
            float maxSpeed = gameObject.Component1.maxSpeed;
            float angularVelocity = gameObject.Component1.angularVelocity * 0.01f; //100 == 1

            moveDir = ProcessInput(moveDir, lerpFac);

            //moveDir = new Vector2(GMath.Lerp(moveDir.X,0,lerpFac/2),GMath.Lerp(moveDir.Y,0,lerpFac/2));

            //NaN protection
            if (-0.0001f <= moveDir.X && moveDir.X <= 0.0001f && -0.0001f <= moveDir.Y && moveDir.Y <= 0.0001f)
            moveDirNormalized = Vector2.Zero;
            else
            moveDirNormalized = moveDir.Normalized;
            

            //Assignment of linear Velocities
            lv.Value.X += moveDir.X * moveSpeed;
            lv.Value.Y += moveDir.Y * moveSpeed;
            //Clamping these values to a maxSpeed
            lv.Value.X = GMath.Clamp(lv.Value.X, -maxSpeed, maxSpeed);
            lv.Value.Y = GMath.Clamp(lv.Value.Y, -maxSpeed, maxSpeed);

            //Handling Rotation! Aligning Grain to moveDir

            //Convert from ZYX Quaternion to angle in radians
            //set player direction
            float angle = Quat2EulerAxisZ(transform.Rotation);
            playerDir = new Vector2(GMath.Cos(angle + (90 * GMath.Deg2Rad)), GMath.Cos(angle));

            Log($"player up direction: {playerDir}");
        }
    }

    private static Vector2 ProcessInput(Vector2 moveDir, float lerpFac)
    {
        //ProcessInput
        if (Input.IsKeyDown(KeyCode.W))
        {
            moveDir.Y = GMath.Lerp(moveDir.Y, 1, lerpFac);
        }
        if (Input.IsKeyDown(KeyCode.S))
        {
            moveDir.Y = GMath.Lerp(moveDir.Y, -1, lerpFac);
        }
        if (Input.IsKeyDown(KeyCode.A))
        {
            moveDir.X = GMath.Lerp(moveDir.X, -1, lerpFac);
        }
        if (Input.IsKeyDown(KeyCode.D))
        {
            moveDir.X = GMath.Lerp(moveDir.X, 1, lerpFac);
        }

        moveDir.X = GMath.Lerp(moveDir.X, 0, lerpFac / 2);
        moveDir.Y = GMath.Lerp(moveDir.Y, 0, lerpFac / 2);
        return moveDir;
    }

    private float Quat2EulerAxisZ(Quaternion quat)
    {
        //To find out how
        //Search up Conversion of ZYX Quaternion to Euler Angle (z-yaw)
        float x = quat.X;
        float y = quat.Y;
        float z = quat.Z;
        float w = quat.W;

        float a = 2 * (w * z + x * y);
        float b = 1 - (2 * ((y * y) + (z * z)));
        float outAngle = GMath.Atan2(a, b);
        return outAngle;
    }

    protected override void OnDestroy()
    {
        Log("System Player destroyed");
    }
}
