using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Collections.Generic;
using System.Runtime;

namespace EchoesBelow.Scripts;

[Component] public record struct InventoryControllerComponent(bool start, int ms01_signifier, int ms02_signifier);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class InventoryController : SystemBase
{
    //I think i only need one instance
    public static InventoryController instance;
    public static ushort ms01_Count;
    public static ushort ms02_Count;

    public static List<ulong> ms01_List;
    public static List<ulong> ms02_List;
    static bool isDown_Q;
    static bool leftSlotIsSelected;
    protected override void OnCreate()
    {
        instance = this;
        Log("System InventoryController initialized");
    }
    private bool OnStart(ref bool startBool)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo
        //Initialize our values!
        ms01_Count = 0;
        ms02_Count = 0;

        leftSlotIsSelected = true;

        //Initialize our lists!
        ms01_List = new List<ulong>();
        ms02_List = new List<ulong>();
        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        //check for input
        isDown_Q = Input.IsKeyDown(KeyCode.Q);

        foreach(var gameObject in World!.Query<InventoryControllerComponent>())
        {
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start);
            //Todo
        }
        //MS01 slot is always first
        if (isDown_Q)
        {
            leftSlotIsSelected = false;
            foreach (var gameObject in World!.Query<GUIElement, MatchSignifierComponent>())
            {
                //Toggle!
                gameObject.Entity.GetFirstChild()!.GetComponent<GUIElement>().Visible = !gameObject.Entity.GetFirstChild().GetComponent<GUIElement>().Visible;
            }
        }


    }
    public void IncrementInStackSlot(int msID)
    {
        switch (msID)
        {
            case 1:
                ms01_Count++;
                foreach(var ui in World!.Query<MatchSignifierComponent>())
                {
                    foreach(var inventory in World!.Query<InventoryControllerComponent>())
                    {
                        if(ui.Component1.signifierID == inventory.Component1.ms01_signifier) 
                        {
                            ui.Entity.GetComponent<GUIText>().TextId = Strings.Intern($"{ms01_Count}");          
                        }
                    }
                }
                break;
            case 2:
                ms02_Count++;
                foreach (var ui in World!.Query<MatchSignifierComponent>())
                {
                    foreach (var inventory in World!.Query<InventoryControllerComponent>())
                    {
                        if (ui.Component1.signifierID == inventory.Component1.ms02_signifier)
                        {
                            ui.Entity.GetComponent<GUIText>().TextId = Strings.Intern($"{ms02_Count}");
                        }
                    }
                }
                break;
        }
        //Log($"ms01 Slot: {ms01_Count}items");
        //Log($"ms02 Slot: {ms02_Count}items");

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


