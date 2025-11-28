using GrapeEngine;
using GrapeEngine.Math;
using GrapeEngine.Numerics;
using GrapeEngine.Scripting;

namespace Scripts.ButtonFolder;

public class CursorTracker : ScriptBehaviour
{
    public static Vector3 cursorRawPos;
    public static CursorTracker singleton;
    //There should always be one cursor only in the scene at all times

    public override void OnStart()
    {
        singleton = this;
    }

    public override void OnUpdate()
    {
        //Store a reference to the camera AND define camera-centric fields
        Entity activeCam = Entity.FromId(CameraController.cameraObjId);
        ref Camera3D activeCamComponent = ref activeCam.GetComponent<Camera3D>();
        float xCamBoundary = activeCamComponent.OrthoSize / 2 * activeCamComponent.AspectRatio;
        float yCamBoundary = activeCamComponent.OrthoSize / 2;

        //offset by half extents width and height, Window origin is at the top left of the screen
        //so we have to offset the origin to the centre of the screen
        float width_halfExtent = Window.Width / 2;
        float height_halfExtent = Window.Height / 2;
        float mouseX = GMath.Clamp((float)Input.MouseX - width_halfExtent, -width_halfExtent, width_halfExtent);
        float mouseY = GMath.Clamp(-(float)Input.MouseY + height_halfExtent, -height_halfExtent, height_halfExtent);

        //Screen to World calculations
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        transform.Position = ScreenToWorld(xCamBoundary, yCamBoundary, mouseX, mouseY);
        

    }

    private Vector3 ScreenToWorld(float xCamBoundary, float yCamBoundary, float mouseX, float mouseY)
    {
        float screenToWorldFacX = xCamBoundary / (Window.Width / 2);
        float screenToWorldFacY = yCamBoundary / (Window.Height / 2);

        float cursorPosX = GMath.Clamp(mouseX * screenToWorldFacX, -xCamBoundary, xCamBoundary);
        float cursorPosY = GMath.Clamp(mouseY * screenToWorldFacY, -yCamBoundary, yCamBoundary);

        //In world space
        cursorRawPos = new Vector3(cursorPosX, cursorPosY, 0);
        return cursorRawPos + CameraController.cameraPos;
    }
}

