using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using GrapeEngine;
using GrapeEngine.Math;


namespace Scripts;

public class ButtonClickable : ScriptBehaviour
{
    public static Vector3 cursorPos;
    public override void OnStart()
    {
        // Called once when the script is initialized
    }

    public override void OnUpdate()
    {
        // Called every frame

        //Find camera boundary
        Entity activeCam = Entity.FromId(CameraController.cameraObjId);
        ref Camera3D activeCamComponent = ref activeCam.GetComponent<Camera3D>();

        float xCamBoundary = activeCamComponent.OrthoSize/ 2 * activeCamComponent.AspectRatio; 
        float yCamBoundary = activeCamComponent.OrthoSize / 2; 

        //offset by half extents
        float mouseX = (float)Input.MouseX - (float)Window.Width / 2;
        float mouseY = -(float)Input.MouseY + (float)Window.Height / 2;
        
        //set limit
        mouseX = GMath.Clamp(mouseX, -Window.Width / 2, Window.Width / 2);
        mouseY = GMath.Clamp(mouseY, -Window.Height / 2, Window.Height / 2);
        
        
        //
        float screenToWorldFacX = xCamBoundary / (Window.Width / 2);
        float screenToWorldFacY = yCamBoundary / (Window.Height / 2);

        float cursorPosX = GMath.Clamp(mouseX * screenToWorldFacX, -xCamBoundary, xCamBoundary);
        float cursorPosY = GMath.Clamp(mouseY * screenToWorldFacY, -yCamBoundary, yCamBoundary);

        //In world space
        cursorPos = new Vector3(cursorPosX, cursorPosY, 0);

        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        transform.Position = cursorPos;
        
        //Log("cursor position: " + cursorPosInWorldSpace);
    }
}

