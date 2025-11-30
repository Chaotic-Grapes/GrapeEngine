using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using GrapeEngine.Math;

namespace Scripts.ObjectPool;

public class MS_Spawner : ScriptBehaviour
{
    float timer = 0;
    float interval = 1f;

    float xBoundaryMax, xBoundaryMin;
    float xValue;

    //iterator limiter
    static int i = 0;
    public override void OnStart()
    {
        // Called once when the script is initialized
        // Define the space where objs spawn
        ref ShapeBox2D boundary = ref GetComponent<ShapeBox2D>();
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        
        //Set the x boundary min and max limits
        xBoundaryMax = transform.Position.X + boundary.HalfExtents.X;
        xBoundaryMin = transform.Position.X - boundary.HalfExtents.X;
    }

    public override void OnUpdate()
    {
        //Declare and assign components
        ref LocalTransform transform = ref GetComponent <LocalTransform>();

        timer += Time.DeltaTime;
        if (timer > interval)
        {
            //Reset timer
            timer = 0;
            //Generate new spawn Coordinate
            xValue = GMath.RandomFloat(xBoundaryMin, xBoundaryMax);
            Vector3 spawnPos = new Vector3(xValue, transform.Position.Y, 0);

            //Teleport MS_Obj to the coordinate
            ulong extractedId = PullFromObjPool();// set obj active to true!
            if (extractedId == 999999) return;

            //Find the obj
            Entity MS_fromObjPool = Entity.FromId(extractedId);
            //Set the new coordinates for the obj
            ref LocalTransform MS_transform = ref MS_fromObjPool.GetComponent<LocalTransform>();
            MS_transform.Position = spawnPos;
            //set the obj to active
            ref Active MS_setActive = ref MS_fromObjPool.GetComponent<Active>();
            MS_setActive.Enabled = true;
        }
    }
    ulong PullFromObjPool()
    {
        //In the future, please use List.Remove
        i++;
        int inPoolCount = MS_ObjPool.ms_Objs.Count;
        int poolLastIndex = inPoolCount - 1; //index of last obj in list
        
        int selectedIndex = GMath.RandomInt(0, poolLastIndex);
        //Log("SELECTED INDEX: " + selectedIndex);

        if (MS_ObjPool.ms_Objs[selectedIndex].instance.inPool)
        {
            i = 0;
            return MS_ObjPool.ms_Objs[selectedIndex].instance.objID;
        }
        else if(i < 5) //5 attempt allowance given for recursive function
        {
            PullFromObjPool();
        }
        else
        {
            i = 0;
            return 999999; //return 999 999 as a default case
        }
        //default case, never accessed
        return 999999;
    }
}

