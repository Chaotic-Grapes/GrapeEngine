using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using Scripts.ButtonFolder;

namespace Scripts.UI_Scripts;

public class Button_Exit : ScriptBehaviour
{
    public static Button_Exit instance;
    Color startCol;
    Button button;
    public ulong objId;
    public override void OnStart()
    {
        instance = this;
        objId = Entity.EntityId;
        //to initialize a button component IMPORTANT example
        ref BoxCollider2D bx = ref GetComponent<BoxCollider2D>();
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        button = new Button(transform.Position, transform, bx.HalfExtents.X, bx.HalfExtents.Y);

        ref SpriteRenderer2D sr = ref GetComponent<SpriteRenderer2D>();
        startCol = sr.Color;
        //Log("Button Created!");
    }

    public override void OnUpdate()
    {
        button.instance.ButtonUpdate();

        ref SpriteRenderer2D sr = ref GetComponent<SpriteRenderer2D>();

        if (button.instance.isPressed)
        {
            
            sr.Color = new Color(100, 100, 100, 255);

            //Quit Application===============================( ? )
            GrapeEngine.Application.Quit(); 
            //End
        }
        else if (button.instance.isHovering)
        {
            sr.Color = new Color(0, 40, 255, 255);
            //Task

            //End
        }
        else
        {
            sr.Color = startCol;
        }

    }
}

