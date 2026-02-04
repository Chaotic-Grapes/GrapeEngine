using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
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

    public static List<ulong>[] objPools;

    public static MS_Manager instance;
    public ulong poolContainerId;

    public static float globalDecayTime;

    public ulong emptyId = 99999999999;
    private Vector3 poolLocation = new Vector3(10000, 10000, 0);

    protected override void OnCreate()
    {
        //This is only ever called once, so there is only one instance assignment
        //initialize
        instance = this;
        Log("System MS_Manager initialized");
        
    }
    private bool OnStart(ref bool startBool, ulong objID, int msID)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo

        switch (msID)
        {
            case 0:
                //For MS Manager instance
                poolContainerId = objID;

                Log("Initialize Pools ! poolContainerId: " + poolContainerId, LogLevel.Debug);

                ms01_ObjectPool = new List<ulong>();
                ms02_ObjectPool = new List<ulong>();
                ms03_ObjectPool = new List<ulong>();
                ms04_ObjectPool = new List<ulong>();
                ms05_ObjectPool = new List<ulong>();
                ms06_ObjectPool = new List<ulong>();
                ms07_ObjectPool = new List<ulong>();

                objPools = new List<ulong>[7];
                objPools[0] = ms01_ObjectPool;
                objPools[1] = ms02_ObjectPool;
                objPools[2] = ms03_ObjectPool;
                objPools[3] = ms04_ObjectPool;
                objPools[4] = ms05_ObjectPool;
                objPools[5] = ms06_ObjectPool;
                objPools[6] = ms07_ObjectPool;
                break;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                SendToPool(objID);
                break;
        }

        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<MS_ManagerComponent>().Without<SpriteRenderer2D>())
        {
            ulong objID = gameObject.Entity.Id;
            int msID = gameObject.Component1.msID;

            //This kinda works like an OnAwake( ) function
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start, objID, msID);

        }


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
        }
    }
    public ulong TakeFromPool(int msID, Vector3 newPos, float decayTime)
    {
        int id_Iterator = 1;
        foreach(List<ulong>objPool in objPools)
        {
            //Check if obj pool is empty
            if (msID == id_Iterator && objPool.Count > 0)
            {
                ulong pulledObjId = objPool[objPool.Count - 1];
                objPool.Remove(pulledObjId);

                InitPoolObj(newPos, pulledObjId, decayTime);

                Log($"Taken from Pool {id_Iterator}!",LogLevel.Debug);
                return pulledObjId;
            }
            id_Iterator++;
        }
        return emptyId;
    }



    public void SendToPool(ulong returningObjId)
    {
        int id_Iterator = 1;
        foreach (List<ulong> objPool in objPools)
        {
            //check if i++ == target msID
            if (Entity.FromId(World!, returningObjId).GetComponent<MS_ManagerComponent>().msID == id_Iterator)
            {
                //add the obj back into the pool, reset its transforms
                objPool.Add(returningObjId);
                ResetPoolObj(returningObjId);

                //returningEntity.GetComponent<Active>().Enabled = false;

                Log($"Sent ms0{Entity.FromId(World!, returningObjId).GetComponent<MS_ManagerComponent>().msID} to Pool {id_Iterator}!", LogLevel.Debug);
                return;
            }
            id_Iterator++;
        }
    }        
    
    private void InitPoolObj(Vector3 newPos, ulong pulledObjId, float decayTime)
    {
        //Set to new transform
        Entity pulledEntity = Entity.FromId(World!, pulledObjId);

        pulledEntity.GetComponent<Active>().Enabled = true;

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
        lv.Value.X = GMath.Random(0.1f, 1f);
        pulledEntity.AddComponent<AngularVelocity2D>();
        //Add Decay Component HARDCODED
        ref MS_DecayComponent decay = ref pulledEntity.AddComponent<MS_DecayComponent>();
        decay.decayTime = decayTime;

    }
    private void ResetPoolObj(ulong objID)
    {
        Entity targetEntity = Entity.FromId(World!, objID);
        ref LocalTransform transform = ref targetEntity.GetComponent<LocalTransform>();
        transform.Position = poolLocation;

        Entity returningEntity = Entity.FromId(World!, objID);
        if (returningEntity.HasComponent<Rigidbody2D>()) returningEntity.RemoveComponent<Rigidbody2D>();
        if (returningEntity.HasComponent<LinearVelocity2D>()) returningEntity.RemoveComponent<LinearVelocity2D>();
        if (returningEntity.HasComponent<AngularVelocity2D>()) returningEntity.RemoveComponent<AngularVelocity2D>();
        if (returningEntity.HasComponent<MS_DecayComponent>()) returningEntity.RemoveComponent<MS_DecayComponent>();

        returningEntity.GetComponent<Active>().Enabled = false;
        //targetEntity.AttachTo(Entity.FromId(World!,poolContainerId));
    }
}
