using GrapeEngine.Numerics;
using GrapeEngine.Scripting;
using Scripts.ButtonFolder;

namespace Scripts.UI_Scripts;

public class MenuManager : ScriptBehaviour
{
    public static bool isPaused = false;
    public static bool isRunning = false;
    public override void OnStart()
    {
        isRunning = false;
    }
    public override void OnUpdate()
    {
        Time.TimeScale = isRunning ? 1 : 0;

        //Log("isRunning: " + isRunning);

        if (!isRunning) return;

        if (Input.IsKeyPressed(KeyCode.Escape))
        {
            isPaused = true;
            
            //Cursor switched on
            Entity cursor = Entity.FromId(CursorTracker.objId);
            ref Active cursorActive = ref cursor.GetComponent<Active>();
            cursorActive.Enabled = true;


            Entity pauseMenu = Entity; //just as a placeholder
            foreach (ulong index in UI_SlideManager.slides)
            {
                pauseMenu = Entity.FromId(index);
                TagMask tag = pauseMenu.GetComponent<TagMask>();
                if (tag.Mask == (int)Tags.PauseMenu) break;
            }
            //placing the menu into the game screen
            ref LocalTransform pauseMenuTransform = ref pauseMenu.GetComponent<LocalTransform>();
            pauseMenuTransform.Position = new Vector3(0, 0, 0);

            //re - enable buttons
            Entity resumeButton = Entity.FromId(Button_Resume.instance.objId);
            ref Active resumeButtonActive = ref resumeButton.GetComponent<Active>();
            resumeButtonActive.Enabled = true;
            Entity exitButton = Entity.FromId(Button_Exit.instance.objId);
            ref Active exitButtonActive = ref exitButton.GetComponent<Active>();
            exitButtonActive.Enabled = true;

            MenuManager.isRunning = false;
            
            //Time.TimeScale = 0;
        }
    }
}
