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
    public List<ulong> ms01_ObjectPool = new List<ulong>();
    public List<ulong> ms02_ObjectPool = new List<ulong>();
    public List<ulong> ms03_ObjectPool = new List<ulong>();
    public List<ulong> ms04_ObjectPool = new List<ulong>();
    public List<ulong> ms05_ObjectPool = new List<ulong>();
    public List<ulong> ms06_ObjectPool = new List<ulong>();
    public List<ulong> ms07_ObjectPool = new List<ulong>();

    protected override void OnCreate()
    {
        Log("System MS_Manager initialized");
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
                Log("Hey my msID is " + Entity.FromId(World,objID).GetComponent<MS_ManagerComponent>().msID);
                break;
            case 2: 
                ms02_ObjectPool.Add(objID);
                Log("Hey my msID is " + Entity.FromId(World, objID).GetComponent<MS_ManagerComponent>().msID);
                break;
            case 3: 
                ms03_ObjectPool.Add(objID);
                break;
            case 4: 
                ms04_ObjectPool.Add(objID);
                break;
            case 5:
                ms05_ObjectPool.Add(objID);
                break;
            case 6:
                ms06_ObjectPool.Add(objID);
                break;
            case 7:
                ms07_ObjectPool.Add(objID);
                break;
            default: //nil
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
                Log($"list 0{++i} count: {ms01_ObjectPool.Count}");
                Log($"list 0{++i} count: {ms02_ObjectPool.Count}");
                Log($"list 0{++i} count: {ms03_ObjectPool.Count}");
                Log($"list 0{++i} count: {ms04_ObjectPool.Count}");
                Log($"list 0{++i} count: {ms05_ObjectPool.Count}");
                Log($"list 0{++i} count: {ms06_ObjectPool.Count}");
                Log($"list 0{++i} count: {ms07_ObjectPool.Count}");
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
    
    protected override void OnDestroy()
    {
        Log("System MS_Manager destroyed");
    }
}
