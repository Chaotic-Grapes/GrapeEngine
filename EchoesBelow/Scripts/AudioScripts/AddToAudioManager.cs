using GrapeEngine.Scripting;

namespace Scripts.AudioScripts;

public class AddToAudioManager : ScriptBehaviour
{
    public override void OnStart()
    {
        Entity entity = new Entity(this.Entity.EntityId);
        // Called once when the script is initialized
        AudioManager.audioEntities.Add(entity);
    }

    public override void OnUpdate()
    {
        // Called every frame
    }
}

