using EchoesBelow.Scripts.Audio;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Collections.Generic;


namespace EchoesBelow.Scripts;

[Component] public record struct ProcessDeathComponent(bool start, float hitCoolDownTimer, float coolDownVal, int cooldownSignifier);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class ProcessDeath : SystemBase
{
    public static bool isHit = false;
    public static bool isDying = false;
    public static ProcessDeath instance;
    static int coolDownSignifier;


    protected override void OnCreate()
    {
        instance = this;
        //Log("System Death initialized", LogLevel.Debug);
    }
    private bool OnStart(ref bool startBool, int signifier)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        isHit = false;
        coolDownSignifier = signifier;
        //entity.GetComponent<ProcessDeathComponent>().timer = 0;

        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {

        foreach(var gameObject in World!.Query<ProcessDeathComponent>())
        {
            //timer is 0 by default
            //Entity entity = Entity.FromId(World!, gameObject.Entity.Id);
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start, gameObject.Component1.cooldownSignifier);


            //if u get hit during the coolDown timer, you die
            //if the cooldown ends, you can repeat the process
            if (isHit)
            {
                gameObject.Component1.hitCoolDownTimer += Time.DeltaTime;

                //convert cooldown to percentage
                float percentage = (gameObject.Component1.coolDownVal - gameObject.Component1.hitCoolDownTimer) / gameObject.Component1.coolDownVal;
                //call signifier
                foreach (var result in World!.Query<MatchSignifierComponent>())
                {
                    if (result.Entity.GetComponent<MatchSignifierComponent>().signifierID == coolDownSignifier)
                    {
                        //set start size
                        result.Entity.GetComponent<GUIElement>().Size.X = 820 * percentage;
                    }
                }


                if(gameObject.Component1.hitCoolDownTimer > gameObject.Component1.coolDownVal)
                {
                    isHit = false;
                    gameObject.Component1.hitCoolDownTimer = 0;
                }
            }


            if (isDying)
            {
                ref LocalTransform transform = ref gameObject.Entity.GetComponent<LocalTransform>();
                transform.Position = new Vector3(GMath.Lerp(transform.Position.X, Checkpoint.checkPointPos.X, 0.04f),
                                                 GMath.Lerp(transform.Position.Y, Checkpoint.checkPointPos.Y, 0.04f), 0f);

                //If my speed is near 0, means Im close to the checkpoint
                //RESPAWN
                float playerXpos = gameObject.Entity.GetComponent<LocalTransform>().Position.X;
                float checkPXpos = Checkpoint.checkPointPos.X;

                float playerYpos = gameObject.Entity.GetComponent<LocalTransform>().Position.Y;
                float checkPYpos = Checkpoint.checkPointPos.Y;

                if (checkPXpos - 0.125f < playerXpos && playerXpos < checkPXpos + 0.125f &&
                    checkPYpos - 0.125f < playerYpos && playerYpos < checkPYpos + 0.125f)
                {
                    gameObject.Entity.GetComponent<Active>().Enabled = true;

                    isDying = false;

                }
            }

        }



    }
    public void TakeHit(ulong otherId, ulong playerId)
    {
        AudioManager.instance.PlaySFX("SFX04");

        Entity other = Entity.FromId(World!, otherId);
        Entity player = Entity.FromId(World!, playerId);

        Vector2 recoilDir = new Vector2(player.GetComponent<LocalTransform>().Position.X - other.GetComponent<LocalTransform>().Position.X,
                                        player.GetComponent<LocalTransform>().Position.Y - other.GetComponent<LocalTransform>().Position.Y);

        player.GetComponent<LinearVelocity2D>().Value = recoilDir.Normalized * 1.5f;

        if (!isHit)
        {
            //set the signifier obj reference ui element!
            foreach(var result in World!.Query<MatchSignifierComponent>())
            {
                if(result.Entity.GetComponent<MatchSignifierComponent>().signifierID == coolDownSignifier)
                {
                    //set start size
                    result.Entity.GetComponent<GUIElement>().Size.X = 338;
                }
            }
            //Log("I'm hit");
            isHit = true;
        }
        else
        {
            isHit = false;
            Death(playerId);
        }
    }
    private void Death(ulong playerId)
    {
        //disable the player
        foreach (var result in World!.Query<MatchSignifierComponent>())
        {
            if (result.Entity.GetComponent<MatchSignifierComponent>().signifierID == coolDownSignifier)
            {
                //set start size
                result.Entity.GetComponent<GUIElement>().Size.X = 0;
            }
        }
        Entity.FromId(World!, playerId).GetComponent<Active>().Enabled = false;
        isDying = true;
        //Log("Death!!!!!!!!!!!!!");

    }
}
