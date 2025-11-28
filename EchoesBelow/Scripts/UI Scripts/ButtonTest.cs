using GrapeEngine.Scripting;
using Scripts.ButtonFolder;


namespace Scripts.UI_Scripts;

public class ButtonTest : ScriptBehaviour
{
    Color startCol;
    Button button;
    public override void OnStart()
    {
        //to initialize a button component IMPORTANT example
        ref BoxCollider2D bx = ref GetComponent<BoxCollider2D>();
        ref LocalTransform transform = ref GetComponent<LocalTransform>();
        button = new Button(transform.Position, bx.HalfExtents.X, bx.HalfExtents.Y);

        ref SpriteRenderer2D sr = ref GetComponent<SpriteRenderer2D>();
        startCol = sr.Color;
        Log("Button Created!");
    }

    public override void OnUpdate()
    {
        button.instance.ButtonUpdate();

        ref SpriteRenderer2D sr = ref GetComponent<SpriteRenderer2D>();

        if (button.instance.isPressed)
        {
            sr.Color = new Color(255, 0, 0, 255);
        }
        else if (button.instance.isHovering)
        {
            sr.Color = new Color(0, 255, 0, 255);
        }
        else
        {
            sr.Color = startCol;
        }

    }
}

