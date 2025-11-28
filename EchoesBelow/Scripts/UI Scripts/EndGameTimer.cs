using GrapeEngine.Numerics;
using GrapeEngine.Scripting;
using Scripts.ButtonFolder;

namespace Scripts.UI_Scripts;

public class EndGameTimer : ScriptBehaviour
{
    float timer = 0.091822645f;
    const float gameEndTime = 2.86118001f; 
    public static bool isEnding = false;

    //TEST
    float timer2 = 0f;
    public override void OnStart()
    {
        // Called once when the script is initialized
        isEnding = false;
    }

    public override void OnUpdate()
    {
        
        if (isEnding == false) return;
        
        timer += Time.UnscaledDeltaTime;
        
        if (timer > gameEndTime)
        {
            //Reset the Player
            Entity playerEntity = Entity.FromId(Player.playerEntityId);
            ref LocalTransform playerTransform = ref playerEntity.GetComponent<LocalTransform>();
            playerTransform.Position = Player.gameStartPos;
            playerTransform.Rotation = Quaternion.Identity;

            //Put the start scene back into game screen
            Entity startScene = Entity; //just as a placeholder
            foreach (ulong index in UI_SlideManager.slides)
            {
                startScene = Entity.FromId(index);
                TagMask tag = startScene.GetComponent<TagMask>();
                if (tag.Mask == (int)Tags.StartScene) break;
            }
            Entity endScene = Entity; //just as a placeholder
            foreach (ulong index in UI_SlideManager.slides)
            {
                endScene = Entity.FromId(index);
                TagMask tag = endScene.GetComponent<TagMask>();
                if (tag.Mask == (int)Tags.EndScene) break;
            }
            
            ref LocalTransform endSceneTransform = ref endScene.GetComponent<LocalTransform>();
            ref LocalTransform startSceneTransform = ref startScene.GetComponent<LocalTransform>();
            //Swap positions
            endSceneTransform.Position = startSceneTransform.Position;
            
            startSceneTransform.Position = new Vector3(0, 0, 0);
            
            //re - enable buttons
            Entity startButton = Entity.FromId(Button_Start.instance.objId);
            ref Active startButtonActive = ref startButton.GetComponent<Active>();
            startButtonActive.Enabled = true;
            
            //Cursor switched on
            CursorOn();
           
            //Reset EndGameTimer
            timer = 0.091822645f;
            isEnding = false;
            MenuManager.isRunning = false;
            // Freeze the Game
            //Time.TimeScale = 0;
            
        }
        
    }
    private static void CursorOn()
    {
        Entity cursor = Entity.FromId(CursorTracker.objId);
        ref Active cursorActive = ref cursor.GetComponent<Active>();
        cursorActive.Enabled = true;
    }
}

