using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using System.Collections.Generic;


namespace EchoesBelow.Scripts.MarineSnowSystem;
[Component] public record struct MS_ManagerComponent(

    int msID,
    bool start

);

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class MS_Manager : SystemBase
{
    //default objs like MS_Managers (non obj pool objs will have an msID of 0)
    public static List<ulong> ms01_ObjectPool;
    public static List<ulong> ms02_ObjectPool;
    public static List<ulong> ms03_ObjectPool;
    public static List<ulong> ms04_ObjectPool;
    public static List<ulong> ms05_ObjectPool;
    public static List<ulong> ms06_ObjectPool;
    public static List<ulong> ms07_ObjectPool;

    public static List<ulong>[] objPools = new List<ulong>[7];

    public static MS_Manager instance;
    public ulong poolContainerId;

    public ulong emptyId = 99999999999;
    private Vector3 poolLocation = new Vector3(10000, 10000, 0);

    protected override void OnCreate()
    {
        Log("System MS_Manager initialized");
        //This is only ever called once, so there is only one instance assignment
        //initialize
        instance = this;
    }
    private bool OnStart(ref bool startBool, ulong objID, int msID)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo

        switch (msID)
        {
            case 1:
                ms01_ObjectPool.Add(objID);
                ResetPoolObjPosAndParent(objID);
                break;
            case 2:
                ms02_ObjectPool.Add(objID);
                ResetPoolObjPosAndParent(objID);
                break;
            case 3:
                ms03_ObjectPool.Add(objID);
                ResetPoolObjPosAndParent(objID);
                break;
            case 4:
                ms04_ObjectPool.Add(objID);
                ResetPoolObjPosAndParent(objID);
                break;
            case 5:
                ms05_ObjectPool.Add(objID);
                ResetPoolObjPosAndParent(objID);
                break;
            case 6:
                ms06_ObjectPool.Add(objID);
                ResetPoolObjPosAndParent(objID);
                break;
            case 7:
                ms07_ObjectPool.Add(objID);
                ResetPoolObjPosAndParent(objID);
                break;
            default: //nil
                //For MS Manager instance
                poolContainerId = objID;
                Log("poolContainerId: " + poolContainerId);

                ms01_ObjectPool = new List<ulong>();
                ms02_ObjectPool = new List<ulong>();
                ms03_ObjectPool = new List<ulong>();
                ms04_ObjectPool = new List<ulong>();
                ms05_ObjectPool = new List<ulong>();
                ms06_ObjectPool = new List<ulong>();
                ms07_ObjectPool = new List<ulong>();

                objPools[0] = ms01_ObjectPool;
                objPools[1] = ms02_ObjectPool;
                objPools[2] = ms03_ObjectPool;
                objPools[3] = ms04_ObjectPool;
                objPools[4] = ms05_ObjectPool;
                objPools[5] = ms06_ObjectPool;
                objPools[6] = ms07_ObjectPool;
                break;
        }




        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        foreach( var gameObject in World!.Query<MS_ManagerComponent>())
        {
            
            ulong objID = gameObject.Entity.Id;
            int msID = gameObject.Component1.msID;

            //A Pseudo Start function, called once per obj at runtime
            //This allows onStart to work
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start, objID, msID);


            if (Input.IsKeyPressed(KeyCode.Q))
            {
                int i = 0;
                Log("============================================");
                foreach(List<ulong> objPool in objPools)
                {
                    Log($"list 0{++i} count: {objPool.Count}");
                }
            }


            switch (msID)
            {
                case 1:
                    

                    break;
                case 2:


                    break;
                case 3:


                    break;
                case 4:


                    break;
                case 5:


                    break;
                case 6:


                    break;
                case 7:


                    break;
                default:
                    break;
            }
        }
    }
    public ulong PullFromPool(int msID, Vector3 newPos)
    {
        int id_Iterator = 1;
        foreach(List<ulong>objPool in objPools)
        {
            //Check if obj pool is empty
            if (msID == id_Iterator && objPool.Count > 0)
            {
                ulong pulledObjId = objPool[objPool.Count - 1];
                objPool.Remove(pulledObjId);
                //Set to new transform
                Entity pulledEntity = Entity.FromId(World!, pulledObjId);
                ref LocalTransform transform = ref pulledEntity.GetComponent<LocalTransform>();
                transform.Position = newPos;
                //Remove from parent
                //pulledEntity.Detach();

                //Add gravity and forces
                ref Rigidbody2D rb = ref pulledEntity.AddComponent<Rigidbody2D>();
                rb.GravityScale = 0.01f;
                rb.Mass = 1;
                rb.LinearDamping = 0.05f;
                rb.Flags = 2u;
                ref LinearVelocity2D lv = ref pulledEntity.AddComponent<LinearVelocity2D>();
                lv.Value.X = GMath.Random(0.1f, 1.8f);
                pulledEntity.AddComponent<AngularVelocity2D>();

                Log($"Pulled from Pool {id_Iterator}!");
                return pulledObjId;
            }
            id_Iterator++;
        }
        return emptyId;
    }
    public void ReturnToPool(ulong returningObjId)
    {
        int id_Iterator = 1;
        foreach (List<ulong> objPool in objPools)
        {
            //check if i++ == target msID
            if (Entity.FromId(World!, returningObjId).GetComponent<MS_ManagerComponent>().msID == id_Iterator)
            {
                //add the obj back into the pool, reset its transforms
                objPool.Add(returningObjId);
                ResetPoolObjPosAndParent(returningObjId);

                Entity returningEntity = Entity.FromId(World!, returningObjId);
                if (returningEntity.HasComponent<Rigidbody2D>()) returningEntity.RemoveComponent<Rigidbody2D>();
                if (returningEntity.HasComponent<LinearVelocity2D>()) returningEntity.RemoveComponent<LinearVelocity2D>();
                if (returningEntity.HasComponent<AngularVelocity2D>()) returningEntity.RemoveComponent<AngularVelocity2D>();

                //returningEntity.GetComponent<Active>().Enabled = false;

                Log($"Returned to Pool {id_Iterator}!");
                return;
            }
            id_Iterator++;
        }
    }    
    private void ResetPoolObjPosAndParent(ulong objID)
    {
        Entity targetEntity = Entity.FromId(World!, objID);
        ref LocalTransform transform = ref targetEntity.GetComponent<LocalTransform>();
        transform.Position = poolLocation;

        //targetEntity.AttachTo(Entity.FromId(World!,poolContainerId));
    }
    protected override void OnDestroy()
    {
        Log("System MS_Manager destroyed");
    }
}
