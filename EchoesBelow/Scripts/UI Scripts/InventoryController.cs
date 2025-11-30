using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using Scripts.ObjectPool;

namespace Scripts.UI_Scripts;

public class InventoryController : ScriptBehaviour
{
    public static InventoryController Instance { get; private set; }

    public static int iterator;
    public static List<Vector3> slotPositions;
    public static List<ulong> capturedMS_inSlots;
    public static List<MS_MarineSnow> storedMSs;
    public static bool isFull = false;
    
    public static Vector3 gameHUDPos;
    public override void OnStart()
    {
        
        Instance = this;

        //All these have to be reset or default at StartScene
        iterator = 0;
        slotPositions = new List<Vector3>();
        capturedMS_inSlots = new List<ulong>();
        storedMSs = new List<MS_MarineSnow>();
        isFull = false;
    }
    public override void OnUpdate()
    {
        

        if (Input.IsKeyPressed(KeyCode.P))
        {
            int i = 0;
            Log("Slots=========================================");
            foreach (Vector3 v in slotPositions)
            {
                //Declare and assign a new reference to the according Obj thru ID
                Log("Slot " + i + ": " + v);
                i++;
            }
            Log("==============================================");
            foreach (ulong u in capturedMS_inSlots)
            {
                //Declare and assign a new reference to the according Obj thru ID
                Log("id: "+ u);
            }
            Log("==============================================");

        }
    }
    public void AddSlotToInvController(Vector3 slotPos)
    {
        slotPositions.Add(slotPos);

        bool swapped;
        do
        {
            swapped = false;
            for (int i = 1; i < slotPositions.Count; i++)
            {
                if (slotPositions[i - 1].X > slotPositions[i].X)
                {
                    // Swap the elements
                    Vector3 temp = slotPositions[i - 1];
                    slotPositions[i - 1] = slotPositions[i];
                    slotPositions[i] = temp;
                    swapped = true;
                }
            }
        } while (swapped);

    }
    public void AddToHotBar(MS_MarineSnow MS, ulong objId)
    {
        if (isFull || iterator > 5) return;
        Log("iterator in AddtoHotbar: " + iterator);
        capturedMS_inSlots.Add(objId);
        storedMSs.Add(MS);
        Entity captured_MS = Entity.FromId(objId);
        ref Active activeStatus = ref captured_MS.GetComponent<Active>();
        //activeStatus.Enabled = false;
        captured_MS.RemoveComponent<BoxCollider2D>();


        ref Layer layer2D = ref captured_MS.GetComponent<Layer>();
        layer2D.Id = 50;

        if (iterator == 5) isFull = true;
        iterator++;
    }

}

