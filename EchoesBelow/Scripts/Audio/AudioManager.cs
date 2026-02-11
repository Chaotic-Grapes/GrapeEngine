using EchoesBelow.Scripts.MarineSnowSystem;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Collections.Generic;

namespace EchoesBelow.Scripts.Audio;

[Component] public record struct AudioManagerComponent(bool start);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class AudioManager : SystemBase
{
    public static AudioManager instance;
    public static List<Entity> sfxEntityList;
    public static Dictionary<string,Entity> sfxEntityDictionary;
    protected override void OnCreate()
    {
        Log("System AudioManager initialized");
    }
    private bool OnStart(ref bool startBool, ulong objId)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo

        instance = this;

        Entity audioManager = Entity.FromId(World!, objId);
        sfxEntityList = new List<Entity>();
        sfxEntityDictionary = new Dictionary<string,Entity>();

        sfxEntityList = audioManager.GetChildren();

        Log($"sfxEntityDictionary has {sfxEntityDictionary.Count} items!");
        foreach(Entity e in sfxEntityList)
        {
            sfxEntityDictionary.Add(e.GetComponent<Name>().Value.ToString(), e);
        }

        //End of Start
        return true;
    }

    protected override void OnUpdate()
    {
        foreach (var gameObject in World!.Query<AudioManagerComponent>())
        {
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start, gameObject.Entity.Id);

            //test 
            if (Input.IsKeyPressed(KeyCode.X))
            {
                foreach (var sfx in sfxEntityDictionary)
                {
                    Log($"I have {sfx} here in this ere list");
                }
            }


        }
    }

    public void PlaySFX(string sfxName)
    {
        Entity chosenSfx = Entity.FromId(World!, sfxEntityDictionary[sfxName].Id);
        ref AudioSource audsrc = ref chosenSfx.GetComponent<AudioSource>();
        audsrc.PlayOnStart = true;
    }
}
