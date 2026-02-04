using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Collections.Generic;

namespace EchoesBelow.Scripts;

[Component] public record struct InventoryControllerComponent(bool start);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class InventoryController : SystemBase
{
    //I think i only need one instance
    public static InventoryController instance;
    public static ushort ms01_Count;
    public static ushort ms02_Count;

    public static List<ulong> ms01_List;
    public static List<ulong> ms02_List;
    protected override void OnCreate()
    {
        instance = this;
        Log("System CamFollow initialized");
    }
    private bool OnStart(ref bool startBool)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        //Initialize our values!
        ms01_Count = 0;
        ms02_Count = 0;

        //Initialize our lists!
        ms01_List = new List<ulong>();
        ms02_List = new List<ulong>();
        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<InventoryControllerComponent>())
        {
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start);
            //Todo
        }

    }
    public void IncrementInStackSlot(int msID)
    {
        switch (msID)
        {
            case 1:
                ms01_Count++;
                break;
            case 2:
                ms02_Count++;
                break;
        }
        Log($"ms01 Slot: {ms01_Count}items");
        Log($"ms02 Slot: {ms02_Count}items");

    }
    public void DecrementInStackSlot(int msID)
    {
        switch (msID)
        {
            case 1:
                ms01_Count--;
                break;
            case 2:
                ms02_Count--;
                break;
        }
        Log($"ms01 Slot: {ms01_Count}items");
        Log($"ms02 Slot: {ms02_Count}items");
    }

}


