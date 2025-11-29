using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using Scripts.ButtonFolder;
using Scripts.Tool_Scripts;

namespace Scripts.UI_Scripts;

public class Button_Start : ScriptBehaviour
{
    public static Button_Start instance;
    Color startCol;
    Button button;
    public ulong objId;
    public override void OnStart()
    {
        //Time.TimeScale = 0;

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

            Entity startMenu = Entity; //just as a placeholder
            foreach (ulong index in UI_SlideManager.slides)
            {
                startMenu = Entity.FromId(index);
                TagMask tag = startMenu.GetComponent<TagMask>();
                if (tag.Mask == (int)Tags.StartScene) break;
            }

            ref LocalTransform startMenuTransform = ref startMenu.GetComponent<LocalTransform>();
            startMenuTransform.Position = new Vector3(0, startMenuTransform.Position.Y + 18, 0);

            Entity gameHUD = Entity; //just as a placeholder
            foreach (ulong index in UI_SlideManager.slides)
            {
                gameHUD = Entity.FromId(index);
                TagMask tag = gameHUD.GetComponent<TagMask>();
                if (tag.Mask == (int)Tags.GameHud) break;
            }

            ref LocalTransform gameHUDTransform = ref gameHUD.GetComponent<LocalTransform>();
            InventoryController.gameHUDPos = new Vector3(0, 0, 0);
            gameHUDTransform.Position = new Vector3(0, 0, 0);

            ref Active active = ref GetComponent<Active>();
            active.Enabled = false;

            //Cursor switched off
            CursorOff();

            //Time.TimeScale = 1;
            MenuManager.isRunning = true;
            Invoke invoke = new Invoke(ref Player.hasCollidedOnce, false, 1f);
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

