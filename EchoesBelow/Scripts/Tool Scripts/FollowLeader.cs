using GrapeEngine.Scripting;
using Scripts.UI_Scripts;

namespace Scripts;

public class FollowLeader : ScriptBehaviour
{
    public override void OnUpdate()
    {
        // Declare and Initialize this transform
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        // Find the attached Entity Tag
        TagMask tag = GetComponent<TagMask>();
        
        // For UI Tag: Follow active Cam
        if (tag.Mask == (uint)Tags.BG) transform.Position = CameraController.cameraPos;

        // For UI Tag: Follow active Player - Change in the future!
        if (tag.Mask == (uint)Tags.UI) transform.Position = Player.playerPos;

        //Example Below
        // For ___ Tag: Follow ___
        //if (tag.Mask == (uint)Tags.___)
        //{
        //transform.Position = ___
        //}
        int i = 0;
        foreach(ulong objId in InventoryController.capturedMS_inSlots)
        {
            Entity captured_MS = Entity.FromId(objId);
            ref LocalTransform MS_transform = ref captured_MS.GetComponent<LocalTransform>();
            MS_transform.Position = CameraController.cameraPos + InventoryController.gameHUDPos + InventoryController.slotPositions[i];
            i++;
        }

    }

}

