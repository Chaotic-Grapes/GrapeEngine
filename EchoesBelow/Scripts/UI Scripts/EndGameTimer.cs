using GrapeEngine.Numerics;
using GrapeEngine.Scripting;
using GrapeEngine.Scripting.Components;
using Scripts.ButtonFolder;
using Scripts.ObjectPool;
using static System.Reflection.Metadata.BlobBuilder;

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
            endSceneTransform.Position = new Vector3(0, 12, 0);

            startSceneTransform.Position = new Vector3(0, 0, 0);
            
            //re - enable buttons
            Entity startButton = Entity.FromId(Button_Start.instance.objId);
            ref Active startButtonActive = ref startButton.GetComponent<Active>();
            startButtonActive.Enabled = true;
            
            //Cursor switched on
            CursorOn();
           
            //Reset Collected MarineSnow items and send them back to the pool
            int i = 0;
            foreach (ulong objId in InventoryController.capturedMS_inSlots)
            {
                //Log("iterator in GameEnd: " + i);
                Entity captured_MS = Entity.FromId(objId);
                ref LocalTransform MS_transform = ref captured_MS.GetComponent<LocalTransform>();
                //reset position
                MS_transform.Position = InventoryController.storedMSs[i].instance.startPos;
                //reset Layer
                ref Layer layer2D = ref captured_MS.GetComponent<Layer>();
                layer2D.Id = (ushort)InventoryController.storedMSs[i].instance.startLayerID;
                //send back to pool
                ref Active activeStatus = ref captured_MS.GetComponent<Active>();
                activeStatus.Enabled = false;
                InventoryController.storedMSs[i].instance.inPool = !activeStatus.Enabled;

                captured_MS.AddComponent<BoxCollider2D>(InventoryController.storedMSs[i].instance.startBX);
                //reset InvController
                
                i++;
            }

            //Send every MS marine snow back to original position
            foreach(MS_MarineSnow ms in MS_ObjPool.ms_Objs)
            {
                Entity msEntity = Entity.FromId(ms.instance.objID);
                ref LocalTransform msTransform = ref msEntity.GetComponent<LocalTransform>();
                ref Active msActive = ref msEntity.GetComponent<Active>();
                msTransform.Position = ms.instance.startPos;
                msActive.Enabled = false;
                ms.instance.inPool = !msActive.Enabled;
            }

            //Reset Inventory
            InventoryController.iterator = 0;
            InventoryController.capturedMS_inSlots.Clear();
            InventoryController.storedMSs.Clear();
            InventoryController.isFull = false;

            //Reset EndGameTimer
            timer = 0.091822645f;
            isEnding = false;
            MenuManager.isRunning = false;

        }
        
    }
    private static void CursorOn()
    {
        Entity cursor = Entity.FromId(CursorTracker.objId);
        ref Active cursorActive = ref cursor.GetComponent<Active>();
        cursorActive.Enabled = true;
    }
}

