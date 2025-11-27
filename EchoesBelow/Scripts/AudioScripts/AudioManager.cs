using GrapeEngine.Scripting;
using GrapeEngine;
using GrapeEngine.Physics;
using GrapeEngine.Numerics;
using GrapeEngine.Math;

namespace Scripts;

public class AudioManager : ScriptBehaviour
{
    public static List<ulong> audioEntityIds = new List<ulong>();
    
    public override void OnStart()
    {
        // Called once when the script is initialized
        //audioEntities = new List<Entity>();
        
    }
    public override void OnUpdate()
    {
        // Called every frame
        if (Input.IsKeyPressed(KeyCode.P))
        {
            //ref AudioSource audio = ref audioEntities[0].GetComponent<AudioSource>();
            //audio.PlayOnStart = true;
            foreach (ulong entityId in audioEntityIds)
            {
                Log($"audioentity: {entityId} detected!");

            }
        }
    }
    //public override void OnDisable()
    //{
    //    audioEntityIds.Clear();
    //    Log("Logging off w disable");
    //}
    //public override void OnDestroy()
    //{
    //    audioEntityIds.Clear();
    //    Log("Logging off w destroy");
    //}
    //public override void OnEnable()
    //{
    //    Log("Enabled!");
    //}
   
}

