using GrapeEngine.Scripting;

namespace Scripts.UI_Scripts;

public class InventorySlot : ScriptBehaviour
{
    public override void OnStart()
    {
        // Called once when the script is initialized
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        InventoryController.Instance.AddSlotToInvController(transform.Position);
    }

    public override void OnUpdate()
    {
        
    }
}

