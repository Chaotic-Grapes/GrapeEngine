using GrapeEngine.Events;
using GrapeEngine.Numerics;
using GrapeEngine.Scripting;

namespace Scripts.ObjectPool;

public class MS_MarineSnow : ScriptBehaviour
{
    //accessible fields
    public ulong objID;
    public bool inPool;
    public MS_MarineSnow instance;

    Vector3 startPos;

    public override void OnStart()
    {
        ref LocalTransform transform = ref GetComponent<LocalTransform>();

        objID = this.Entity.EntityId;
        instance = this;
        startPos = transform.Position;
        
        // assign this field
        MS_ObjPool.ms_Objs.Add(this);
        
        //this allows the above code to run on start
        //and it allows this script to be initalized in the first place
        //then it promptly sets it to inactive
        ref Active activeStatus = ref GetComponent<Active>();
        activeStatus.Enabled = false;
        inPool = !activeStatus.Enabled;
    }
    public override void OnUpdate()
    {
        ref LocalTransform transform = ref GetComponent<LocalTransform>();

        //If obj is active, it is not in pool, vice versa
        ref Active activeStatus = ref GetComponent<Active>();
        inPool = !activeStatus.Enabled;

        // Get collision events for this wall
        List<CollisionEvent> events = CollisionEvents.GetEvents(Entity);

        if(events.Count != 0) //only check for first collision
        {
            if (events[0].Other.EntityId == Player.playerEntityId)
            {
                //future send to inv controller
                //send it back to its original position and reset all values
                transform.Position = startPos;
                activeStatus.Enabled = false;
                inPool = !activeStatus.Enabled;
            }
            else if (events[0].Type == CollisionEventType.Stay)
            {
                //send it back to its original position and reset all values
                transform.Position = startPos;
                activeStatus.Enabled = false;
                inPool = !activeStatus.Enabled;
            }
        }
        
        


    }
    public override void OnCollisionEnter(ulong otherEntity)
    {
        base.OnCollisionEnter(otherEntity);
        Log("1 COLLIDE");
    }
    public override void OnCollisionExit(ulong otherEntity)
    {
        base.OnCollisionExit(otherEntity);
        Log("2 COLLIDE");
    }
    public override void OnCollisionStay(ulong otherEntity)
    {
        base.OnCollisionStay(otherEntity);
        Log("3 COLLIDE");
    }
}

