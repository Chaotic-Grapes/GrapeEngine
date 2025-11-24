using GrapeEngine.Scripting;

namespace Scripts;

public class TestMove : ScriptBehaviour
{
    float moveSpeed = 2f;
    
    public override void OnStart()
    {
        // Called once when the script is initialized
        
    }

    public override void OnUpdate()
    {
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        if (Input.IsKeyDown(KeyCode.W))
        {
            transform.Position.Y += moveSpeed * Time.DeltaTime;
        }
        if (Input.IsKeyDown(KeyCode.S))
        {
            transform.Position.Y -= moveSpeed * Time.DeltaTime;
        }
        if (Input.IsKeyDown(KeyCode.D))
        {
            transform.Position.X += moveSpeed * Time.DeltaTime;
        }
        if (Input.IsKeyDown(KeyCode.A))
        {
            transform.Position.X -= moveSpeed * Time.DeltaTime;
        }

    }
    

   
}

