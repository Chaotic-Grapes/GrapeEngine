using GrapeEngine.Events;
using GrapeEngine.Math;
using GrapeEngine.Numerics;
using GrapeEngine.Physics;
using GrapeEngine.Scripting;
using Scripts.ObjectPool;
using Scripts.UI_Scripts;

namespace Scripts;

public class Player : ScriptBehaviour
{
    //Constant Parameters
    const float moveSpeed = 2.2f;
    const float maxSpeed = 30f;
    const float rotateSpeed = 200f;
    
    
    //For Testing Purposes
    
    // static reference that can be editted in this file.
    public static ulong playerEntityId = 0;

    //These are publicly available fields
    public static Vector3 gameStartPos; // for resetting the game
    public static Vector3 playerPos;
    public static Vector2 playerDir;

    //Hail-Mary Solution, this is like a bandage 
    public static bool hasCollidedOnce = false;
    

    //AngularVelocity2D av;
    public override void OnStart()
    {
        hasCollidedOnce = false;
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        gameStartPos = transform.Position;
        // Called once when the script is initialized
        playerEntityId = EntityId;  
        Log($"Player initialized with Entity ID: {playerEntityId}"); 
        //av = GetComponent<AngularVelocity2D>();
        //try another day

    }
    public override void OnUpdate()
    {
        if (CollisionEvents.GetEvents(Entity).Count == 0) hasCollidedOnce = false;

        //Declare and initialize components
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        ref LinearVelocity2D linearVelocity = ref GetComponent<LinearVelocity2D>();
        ref AngularVelocity2D angularVelocity = ref GetComponent<AngularVelocity2D>();
        //ref AudioSource ads = ref GetComponent<AudioSource>();
        //Log("AudioSource: " + ads.Volume);
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

        //Reach endgame Object
        EndGameQuery();
        if (InventoryController.capturedMS_inSlots.Count >= 5) InventoryController.isFull = true;

        //Log("playerDir: " + playerDir + " angle: " + angle);
    }

    private void EndGameQuery()
    {
        
        List<CollisionEvent> collisionEvents = CollisionEvents.GetEvents(Entity);
        //Log("CollisionCount: " + collisionEvents.Count);
        if (CollisionEvents.GetEvents(Entity).Count != 0 && hasCollidedOnce == false) //&& collisionEvents[collisionEvents.Count-1].Type == CollisionEventType.Enter)
        {
            hasCollidedOnce = true;
            Entity other = Entity.FromId(collisionEvents[0].Other.EntityId);
            ref TagMask otherGameObjectTag = ref other.GetComponent<TagMask>();
            //was feeling nostalgic
            if ((int)otherGameObjectTag.Mask == (int)Tags.EndGameObj) //its 9
            {
                Entity endScene = Entity; //just as a placeholder
                foreach (ulong index in UI_SlideManager.slides)
                {
                    endScene = Entity.FromId(index);
                    TagMask tag = endScene.GetComponent<TagMask>();
                    if (tag.Mask == (int)Tags.EndScene) break;
                }
                //placing the endScene into the game screen
                ref LocalTransform endSceneTransform = ref endScene.GetComponent<LocalTransform>();
                endSceneTransform.Position = new Vector3(0, 0, 0);

                Entity gameHUD = Entity; //just as a placeholder
                foreach (ulong index in UI_SlideManager.slides)
                {
                    gameHUD = Entity.FromId(index);
                    TagMask tag = gameHUD.GetComponent<TagMask>();
                    if (tag.Mask == (int)Tags.GameHud) break;
                }

                ref LocalTransform gameHUDTransform = ref gameHUD.GetComponent<LocalTransform>();
                InventoryController.gameHUDPos = new Vector3(0, gameHUDTransform.Position.Y + 24, 0);
                gameHUDTransform.Position = new Vector3(0, gameHUDTransform.Position.Y + 24, 0);


                EndGameTimer.isEnding = true;
                MenuManager.isRunning = false;
                //CollisionEvents.GetEvents(Entity).Clear();
            }
            
        }
        
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
        //currently adding force continuously for now
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



