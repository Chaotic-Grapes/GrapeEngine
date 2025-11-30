using GrapeEngine.Scripting;

namespace Scripts.AudioScripts;

public class AddToAudioManager : ScriptBehaviour
{
    public override void OnStart()
    {

        AudioManager.audioEntityIds.Add(this.Entity.EntityId);

        //Log("start");
        //ref AudioSource asource = ref GetComponent<AudioSource>();
        //Log("Entity Itself: " +  asource.Pitch);
        //Log("1");
        //Entity entity = new Entity(this.Entity.EntityId);
        //ref AudioSource asource2 = ref entity.GetComponent<AudioSource>();
        //Log("New Entity from Id Itself: " + asource2.Pitch);
        //Log("2");
        //Entity test = Entity.FromId(Entity.EntityId);
        //ref AudioSource asource3 = ref test.GetComponent<AudioSource>();
        //Log("Entity FromId(): " + asource3.Pitch);
        //Log("3");
    }

    public override void OnUpdate()
    {
        // Called every frame
        
        //ref AudioSource asource = ref GetComponent<AudioSource>();
        //Log("Entity Itself: " + asource);

        //Entity entity = new Entity(this.Entity.EntityId);
        //ref AudioSource asource2 = ref entity.GetComponent<AudioSource>();
        //Log("New Entity from Id Itself: " + asource2);

        //Entity test = Entity.FromId(Entity.EntityId);
        //ref AudioSource asource3 = ref test.GetComponent<AudioSource>();
        //Log("Entity FromId(): " + asource3);
    }
}

