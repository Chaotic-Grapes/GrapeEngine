using GrapeEngine;
using GrapeEngine.Math;
using GrapeEngine.Numerics;
using GrapeEngine.Scripting;

namespace Scripts.ButtonFolder;

public class Button : ScriptBehaviour
{
    public bool isHovering { get; set; }
    public bool isPressed { get; set; }
    public Button instance;

    //private fields
    private float halfExtent_X;
    private float halfExtent_Y;
    private Vector3 buttonPos;
    //private Vector2 v0, v1, v2, v3;
    private float xMin, xMax, yMin, yMax;

    //Constructor
    public Button(Vector3 buttonPos, LocalTransform transform, float halfExtent_X, float halfExtent_Y)
    {
        this.buttonPos = buttonPos;
        this.halfExtent_X = halfExtent_X * transform.Scale.X;
        this.halfExtent_Y = halfExtent_Y * transform.Scale.Y;

        instance = this;
        Log("Hello I am a button!");
    }
    public override void OnStart()
    {
        if (CursorTracker.singleton == null) Log("There is no CursorTracker Obj found!", LogLevel.Error);
    }

    public void ButtonUpdate()
    {
        xMax = buttonPos.X + halfExtent_X;
        xMin = buttonPos.X - halfExtent_X;
        yMax = buttonPos.Y + halfExtent_Y;
        yMin = buttonPos.Y - halfExtent_Y;

        //Log("CursorPos Now via Button: " + CursorTracker.cursorRawPos);
        //Log($"xMin [{xMin}]<: " + (xMin <= CursorTracker.cursorRawPos.X));
        //Log($"xMax [{xMax}]>: " + (xMax >= CursorTracker.cursorRawPos.X));
        //Log($"yMin [{yMin}]<: " + (yMin <= CursorTracker.cursorRawPos.Y));
        //Log($"yMax [{yMax}]>: " + (yMax >= CursorTracker.cursorRawPos.Y));

        if (xMin <= CursorTracker.cursorRawPos.X
         && xMax >= CursorTracker.cursorRawPos.X
         && yMin <= CursorTracker.cursorRawPos.Y
         && yMax >= CursorTracker.cursorRawPos.Y)
        {
            //Log("1: StepButton");
            isHovering = true;
            if (Input.IsMousePressed(0)) isPressed = true;
            else isPressed = false;
        }
        else
        {
            //Log("2: StepButton");
            isHovering = false;
            isPressed = false;
        }
    }
}

