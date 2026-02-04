using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using EchoesBelow.Scripts.MarineSnowSystem;

namespace EchoesBelow.Scripts;

[Component] public record struct PlayerComponent(
    //[Pseudo-SerializeField]
    float driftSpeed,
    float periodicForceInterval,
    float moveSpeed,
    float angularVelocity,
    bool start // required for start function
);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class Player : SystemBase
{
    public static Player instance;

    public static Compass abs_InputDirection = Compass.N;
    public Vector3 currentPos;
    const float lerpFac = 1;
    const float maxSpeed = 8;
    float timer_forRotation = 0;
    float timer_forPeriodicForce = 0;
    float dashCoolDownTimer;
    bool isCoolingDown;

    static bool isKeyDown_W = false;
    static bool isKeyDown_A = false;
    static bool isKeyDown_S = false;
    static bool isKeyDown_D = false;

    protected override void OnCreate()
    {
        instance = this;
        Log("System Player initialized");
    }
    private bool OnStart(ref bool startBool)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo

        dashCoolDownTimer = 0;
        isCoolingDown = false;




        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        isKeyDown_W = Input.IsKeyDown(KeyCode.W);
        isKeyDown_A = Input.IsKeyDown(KeyCode.A); 
        isKeyDown_S = Input.IsKeyDown(KeyCode.S);
        isKeyDown_D = Input.IsKeyDown(KeyCode.D);

        foreach(var gameObject in World!.Query<PlayerComponent, LinearVelocity2D, AngularVelocity2D, LocalTransform>())
        {
            //A Pseudo Start function, called once per obj at runtime
            //This allows onStart to work
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start);

            //Variables
            ref LocalTransform transform = ref gameObject.Component4;
            ref LinearVelocity2D lv = ref gameObject.Component2;
            ref AngularVelocity2D av = ref gameObject.Component3;
            Vector2 playerDir;
            Vector2 moveDir = Vector2.Zero;
            Vector2 moveDirNormalized = Vector2.Zero;
            float moveSpeed = gameObject.Component1.moveSpeed * 0.01f; //this allows floating point decimal values
            float driftSpeed = gameObject.Component1.driftSpeed * 0.01f; //this allows floating point decimal values
            float angularVelocity = gameObject.Component1.angularVelocity * 0.01f; //100 == 1
            float periodicForceInterval = gameObject.Component1.periodicForceInterval;

            moveDir = ProcessInput(moveDir, lerpFac);

            //NaN protection for normalization
            if (-0.0001f <= moveDir.X && moveDir.X <= 0.0001f && -0.0001f <= moveDir.Y && moveDir.Y <= 0.0001f) moveDirNormalized = Vector2.Zero;
            else moveDirNormalized = moveDir.Normalized;

            //Handling Rotation! Aligning Grain to moveDir=================================================
            //Convert from ZYX Quaternion to angle in radians
            //Find the local "Up" Vector of the Player. Think of this as gameObject.transform.up in Unity
            float playerAngle = Quat2EulerAxisZ(transform.Rotation);
            playerDir = new Vector2(GMath.Cos(playerAngle + (90 * GMath.Deg2Rad)), GMath.Cos(playerAngle));

            //Find change in angle required using dot product between moveDirNormalized and playerDir
            //NaN protection when player is facing up or at rest
            if (-0.0001f < playerDir.X && playerDir.X < 0.0001f && 0.9999f < playerDir.Y && playerDir.Y < 1.0001f) playerDir = new Vector2(0, 1);

            //============================================================================================
            RotationPolarityHandler(transform);
            float flipFactor = GMath.Clamp(HeadingDifference(playerAngle * GMath.Rad2Deg, (float)abs_InputDirection) * GMath.Rad2Deg, -1,1);
            
            //Dot product operation to determine theta as presented by angleBetween in radians!
            float angleBetween_rad = GMath.Acos(GMath.Dot(playerDir, moveDirNormalized) / (playerDir.Magnitude * moveDirNormalized.Magnitude));
            angleBetween_rad = (float.IsNaN(angleBetween_rad)) ? 0 : angleBetween_rad;

            //Find change in time required to complete a rotation. This formula requires radians
            //Must always be positive so we use Magnitude thru Abs
            float rotDuration = GMath.Abs(angleBetween_rad / angularVelocity);
            
            //start Rotation process
            bool isRotating = false;
            if (angleBetween_rad != 0) 
            { 
                isRotating = true; 
            } 
            if (isRotating)
            {
                timer_forRotation += Time.DeltaTime;
                if(flipFactor >= 0) av.Value = GMath.Lerp(av.Value, angularVelocity, lerpFac);
                else                av.Value = GMath.Lerp(av.Value, -angularVelocity, lerpFac);
            }
            if (timer_forRotation > rotDuration)
            {
                isRotating = false;
                timer_forRotation = 0;
                av.Value = 0;
            }

            //Log($"playerAngle: {playerAngle}");
            //Log($"absTarget: {abs_InputDirection}");
            //Log("flipFac2: " + flipFac);

            if (isKeyDown_W || isKeyDown_S
            || isKeyDown_A || isKeyDown_D)
            {
                AddDriftForce(ref lv, playerDir, driftSpeed, maxSpeed);
                AddPeriodicalForce(ref lv, periodicForceInterval, ref timer_forPeriodicForce, playerDir, moveSpeed);
            }
            if(Input.IsKeyPressed(KeyCode.Space) && !isCoolingDown)
            { 
                AddRelativeForce(ref lv, playerDir, moveSpeed);
                isCoolingDown = true;
                dashCoolDownTimer = 1.5f;
            }
            if (isCoolingDown)
            {
                dashCoolDownTimer -= Time.DeltaTime;
                if(dashCoolDownTimer < 0)
                {
                    isCoolingDown = false;
                }
            }


            //Finally
            SpeedLimit(ref lv, maxSpeed);

            //update Position
            currentPos = transform.Position;
        }
    }
    private void SpeedLimit(ref LinearVelocity2D lv,float maxSpeed)
    {
        lv.Value.X = GMath.Clamp(lv.Value.X, -maxSpeed, maxSpeed);
        lv.Value.Y = GMath.Clamp(lv.Value.Y, -maxSpeed, maxSpeed);
    }
    private void AddRelativeForce(ref LinearVelocity2D lv, Vector2 playerDir, float moveSpeed)
    {
        lv.Value.X += playerDir.X * moveSpeed * 2 * GMath.Clamp(lv.Value.X, 1, 10);
        lv.Value.Y += playerDir.Y * moveSpeed * 2 *  GMath.Clamp(lv.Value.X, 1, 10);
    }
    private static void AddDriftForce(ref LinearVelocity2D lv, Vector2 playerDir, float moveSpeed, float maxSpeed)
    {
        lv.Value.X += playerDir.X * moveSpeed;
        lv.Value.Y += playerDir.Y * moveSpeed;
        //Clamping these values to a maxSpeed
        lv.Value.X = GMath.Clamp(lv.Value.X, -maxSpeed, maxSpeed);
        lv.Value.Y = GMath.Clamp(lv.Value.Y, -maxSpeed, maxSpeed);
       
    }
    private void AddPeriodicalForce(ref LinearVelocity2D lv, float periodicForceInterval, ref float timer_forPeriodicForce, Vector2 playerDir, float moveSpeed)
    {
        //The periodical force is applied 
        timer_forPeriodicForce += Time.DeltaTime;
        if(timer_forPeriodicForce > periodicForceInterval)
        {
            timer_forPeriodicForce = 0;
            lv.Value.X += playerDir.X * moveSpeed * GMath.Clamp(lv.Value.X, 1, 10);
            lv.Value.Y += playerDir.Y * moveSpeed * GMath.Clamp(lv.Value.X, 1, 10);
        }
    }
    private void RotationPolarityHandler(LocalTransform transform)
    {
        //Find InputAbsDirection direction
        //For a quirk in the angles, Keycodes D and A (left and right) are swapped!
        if (isKeyDown_W && isKeyDown_D) abs_InputDirection = Compass.NW;
        else if (isKeyDown_S && isKeyDown_D) abs_InputDirection = Compass.SW;
        else if (isKeyDown_S && isKeyDown_A) abs_InputDirection = Compass.SE;
        else if (isKeyDown_W && isKeyDown_A) abs_InputDirection = Compass.NE;
        else if (isKeyDown_W) abs_InputDirection = Compass.N;
        else if (isKeyDown_D) abs_InputDirection = Compass.W;
        else if (isKeyDown_S) abs_InputDirection = Compass.S;
        else if (isKeyDown_A) abs_InputDirection = Compass.E;
    }

    private Vector2 ProcessInput(Vector2 moveDir, float lerpFac)
    {
        if (isKeyDown_W) moveDir.Y = GMath.Lerp(moveDir.Y, 1, lerpFac);
        if (isKeyDown_S) moveDir.Y = GMath.Lerp(moveDir.Y, -1, lerpFac);
        if (isKeyDown_A) moveDir.X = GMath.Lerp(moveDir.X, -1, lerpFac);
        if (isKeyDown_D) moveDir.X = GMath.Lerp(moveDir.X, 1, lerpFac);
        //Always decelerrating moveDir but at a slower rate
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
    private float HeadingDifference(float heading1, float heading2)
    {
        float diff = (heading2 - heading1 + 180) % 360 - 180;
        return diff < -180 ? diff + 360 : diff;
    }
    protected override void OnDestroy()
    {
        Log("System Player destroyed");
    }
    public enum Compass
    {
        NE = 45,
        E  = 90,
        SE = 135,
        S  = 180,
        SW = 225,
        W  = 270,
        NW = 315,
        N  = 0
    }
}

//[Component] public record struct PlayerCollisionHandler();
[System(SystemGroup.PostPhysics, SystemRunMode.PlayOnly)]
public class PlayerCollisionHandler : CollisionSystemBase
{
    protected override void OnCreate()
    {
        Log("System CollisionTest initialized");
    }

    protected override void OnCollisionEnter(Entity self, CollisionEvent evt)
    {
        base.OnCollisionEnter(self, evt);

        if (self.HasComponent<PlayerComponent>()) CollisionEntered(self, evt);
    }

    private void CollisionEntered(Entity self, CollisionEvent evt)
    {
        //do Everything in here
        Log($"{self.GetComponent<Name>().ToString()} collided with {Entity.FromId(World!, evt.OtherEntityId).GetComponent<Name>().ToString()} at {evt.ContactPoint}",LogLevel.Debug);
        Entity other = Entity.FromId(World!, evt.OtherEntityId);
        if (other.HasComponent<MS_ManagerComponent>())
        {
            MS_Manager.instance.SendToPool(other.Id);
            InventoryController.instance.IncrementInStackSlot(other.GetComponent<MS_ManagerComponent>().msID);
        }
    }

    protected override void OnCollisionExit(Entity self, CollisionExitEvent evt)
    {
        base.OnCollisionExit(self, evt);
        //foreach (var gameObject in World!.Query<CollisionTestComponent>())
        //{
        //    Log("Hello I Exited");
        //}
    }

}
