using GrapeEngine.Events;
using GrapeEngine.Numerics;
using GrapeEngine.Scripting;
using Scripts.UI_Scripts;

namespace Scripts.ObjectPool;

public class MS_MarineSnow : ScriptBehaviour
{
    //accessible fields
    public ulong objID;
    public bool inPool;
    public MS_MarineSnow instance;

    public ulong startLayerID;
    public Vector3 startPos;
    public BoxCollider2D startBX;

    float timer = 0f;
    bool isTransitioning = false;

    public override void OnStart()
    {
        isTransitioning = false;
        timer = 0f;
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        BoxCollider2D bx = GetComponent<BoxCollider2D>();

        startBX = bx;
        objID = this.Entity.EntityId;
        instance = this;
        startPos = transform.Position;
        ref Layer layer2D = ref GetComponent<Layer>();
        startLayerID = layer2D.Id;
        
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
        if (isTransitioning) timer += Time.UnscaledDeltaTime;
        if (timer > 0.5f)
        {
            timer = 0f;
            isTransitioning = false;
        }
        ref LocalTransform transform = ref GetComponent<LocalTransform>();

        //If obj is active, it is not in pool, vice versa
        ref Active activeStatus = ref GetComponent<Active>();
        inPool = !activeStatus.Enabled;

        SendBackToPool(ref transform, ref activeStatus);
    }

    private void SendBackToPool(ref LocalTransform transform, ref Active activeStatus)
    {
        // Get collision events for this wall
        List<CollisionEvent> collisionEvents = CollisionEvents.GetEvents(Entity);

        if (collisionEvents.Count != 0) //only check for first collision
        {
            if (!isTransitioning && collisionEvents[0].Other.EntityId == Player.playerEntityId)
            {
                isTransitioning = true;
                //future send to inv controller
                InventoryController.Instance.AddToHotBar(instance, Entity.EntityId);
                Log("Added to hotbar");
                
                //disable

            }
            else if (collisionEvents[0].Type == CollisionEventType.Stay)
            {
                //send it back to its original position and reset all values
                transform.Position = startPos;
                activeStatus.Enabled = false;
                inPool = !activeStatus.Enabled;
            }
        }
    }
}

