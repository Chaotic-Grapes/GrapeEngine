using GrapeEngine.Scripting;
using GrapeEngine;
using GrapeEngine.Physics;
using GrapeEngine.Numerics;
using GrapeEngine.Math;

namespace Scripts;

public class AudioManager : ScriptBehaviour
{
    public static List<Entity> audioEntities = new List<Entity>();
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
            foreach (Entity entity in audioEntities)
            {
                Log("Inside: " + entity.EntityId);
                Entity e = Entity.FromId(entity.EntityId);
                ref AudioSource audioClip = ref e.GetComponent<AudioSource>();
                Log("Audio: " +  audioClip + " | Volume: " + audioClip.Volume);

            }
        }
    }
}

