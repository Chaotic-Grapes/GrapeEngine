using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using Scripts.ButtonFolder;

namespace Scripts.UI_Scripts;

public class Button_Resume : ScriptBehaviour
{
    public static Button_Resume instance;
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
            //Task

            Entity pauseMenu = Entity; //just as a placeholder
            foreach (ulong index in UI_SlideManager.slides)
            {
                pauseMenu = Entity.FromId(index);
                TagMask tag = pauseMenu.GetComponent<TagMask>();
                if (tag.Mask == (int)Tags.PauseMenu) break;
            }

            ref LocalTransform pauseMenuTransform = ref pauseMenu.GetComponent<LocalTransform>();
            pauseMenuTransform.Position = new Vector3(pauseMenuTransform.Position.X, pauseMenuTransform.Position.Y + 15, 0);

            //wow...
            ref Active active = ref GetComponent<Active>();
            active.Enabled = false;
            Entity exitButton = Entity.FromId(Button_Exit.instance.objId);
            ref Active exitButtonActive = ref exitButton.GetComponent<Active>();
            exitButtonActive.Enabled = false;

            //Cursor switched off
            CursorOff();

            MenuManager.isRunning = true;
            //Time.TimeScale = 1;
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

    private static void CursorOff()
    {
        Entity cursor = Entity.FromId(CursorTracker.objId);
        ref Active cursorActive = ref cursor.GetComponent<Active>();
        cursorActive.Enabled = false;
    }
}

