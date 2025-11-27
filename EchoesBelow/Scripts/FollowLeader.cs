using GrapeEngine.Scripting;

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
        if (tag.Mask == (uint)Tags.UI) transform.Position = CameraController.cameraPos;
        
        //Example Below
        // For ___ Tag: Follow ___
        //if (tag.Mask == (uint)Tags.___)
        //{
            //transform.Position = ___
        //}
        

    }
}

