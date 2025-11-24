using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace GameScripts;

//List of Directives


public class TestScript : ScriptBehaviour
{
    float timer = 5f;
    float moveSpeed = 2f;
    
    public override void OnStart()
    {
        // Called once when the script is initialized
        // use ref if you want to update any changes on the component
        ref var transform = ref GetComponent<LocalTransform>();


        transform.Position = new Vector3(0,0,0);
    }

    public override void OnUpdate()
    {
        ref var transform = ref GetComponent<LocalTransform>();

        // Called every frame
        timer -= Time.DeltaTime;
        if (timer < 0) 
        {
            Log("RESET timer");
            timer = 5f;
        }

        if(timer > 0)
        {
            //It should be uppercase (default C# style is pascal case) Unity just wna be different
            //Unity wasnt C# initially
            transform.Position.Y += 0.2f * Time.DeltaTime * moveSpeed;
        }
    }
}


