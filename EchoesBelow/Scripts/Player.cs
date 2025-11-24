using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using GrapeEngine.Math;

namespace Scripts;

public class Player : ScriptBehaviour
{
    //Constant Parameters
    const float moveSpeed = 2f;
    const float torqueMax = 3f;
    const float torqueSpeed = 1f;

    //For Testing Purposes
    float torqueInitial = 0f;



    //Test out alternative to serializable fields!
    public static Vector3 playerPos;
    
    public override void OnStart()
    {
        // Called once when the script is initialized
    }
    public override void OnUpdate()
    {
        //Declare and initialize components
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        ref AngularVelocity2D torque = ref GetComponent<AngularVelocity2D>();


        if (Input.IsKeyDown(KeyCode.W))
        {   //Up
            transform.Position.Y += moveSpeed * Time.DeltaTime;
        }
        if (Input.IsKeyDown(KeyCode.S))
        {   //Down
            transform.Position.Y -= moveSpeed * Time.DeltaTime;
        }   
        if (Input.IsKeyDown(KeyCode.D))
        {   //Right
            //torqueInitial = Torque(torqueInitial);
            transform.Position.X += moveSpeed * Time.DeltaTime;
        }
        if (Input.IsKeyDown(KeyCode.A))
        {   //Left
            transform.Position.X -= moveSpeed * Time.DeltaTime;
        }

        //store player position for other scripts to find
        playerPos = transform.Position;

        //Logs
        Log("torqueInitial: " + torqueInitial);
    }
    public float Torque(float currentTorque)
    {
        currentTorque = GMath.Lerp(currentTorque, torqueMax, torqueSpeed * Time.DeltaTime);
        return 0f;
    }
}



