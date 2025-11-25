using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using GrapeEngine.Math;
using GrapeEngine.Physics;

namespace Scripts;

public class Player : ScriptBehaviour
{
    //Constant Parameters
    const float moveSpeed = 1f;
    const float maxSpeed = 50f;
    const float rotateSpeed = 70f;
    

    //For Testing Purposes
    
    

    //These are publicly available fields
    public static Vector3 playerPos;
    public static Vector2 playerDir;

    //AngularVelocity2D av;
    public override void OnStart()
    {
        // Called once when the script is initialized

        //av = GetComponent<AngularVelocity2D>();
        //try another day
    }
    public override void OnUpdate()
    {
        //Declare and initialize components
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        ref LinearVelocity2D linearVelocity = ref GetComponent<LinearVelocity2D>();
        ref AngularVelocity2D angularVelocity = ref GetComponent<AngularVelocity2D>();
        
        
        if (Input.IsKeyDown(KeyCode.W))
        {   //Up
            //linearVelocity.Value = playerDir * moveSpeed * Time.DeltaTime;
            AddForce(ref linearVelocity);
        }
        if (Input.IsKeyDown(KeyCode.S))
        {   //Down
            
        }   
        if (Input.IsKeyDown(KeyCode.D))
        {   //Right Key
            AddTorque(ref angularVelocity, -1); //positive rotate direction
        }
        if (Input.IsKeyDown(KeyCode.A))
        {   //Left Key
            AddTorque(ref angularVelocity, 1);//negative rotate direction
        }

        //store player position for other scripts to find
        playerPos = transform.Position;

        //Convert from ZYX Quaternion to angle in radians
        float angle = Quat2EulerAxisZ(transform.Rotation);
        //set player direction
        playerDir = new Vector2(GMath.Cos(angle + (90 * GMath.Deg2Rad)), GMath.Cos(angle));

        
        //Log("playerDir: " + playerDir + " angle: " + angle);
    }
    public float AddTorque(ref AngularVelocity2D angularVelocity, float input)
    {
        //Clamp the xInput between -1 and 1
        input = GMath.Clamp(input, -1, 1f);

        //reference the angular velocity property
        //ref AngularVelocity2D angularVelocity = ref GetComponent<AngularVelocity2D>();

        angularVelocity.Value = GMath.Lerp(angularVelocity.Value, input * rotateSpeed * Time.DeltaTime, 0.2f);
        return angularVelocity.Value;
    }
    private void AddForce(ref LinearVelocity2D linearVelocity)
    { //add an impulse force
        linearVelocity.Value += playerDir * moveSpeed * Time.DeltaTime;
        linearVelocity.Value.X = GMath.Clamp(linearVelocity.Value.X, -maxSpeed, maxSpeed);
        linearVelocity.Value.Y = GMath.Clamp(linearVelocity.Value.Y, -maxSpeed, maxSpeed);
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
}



