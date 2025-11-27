using GrapeEngine.Scripting;

namespace Scripts.ObjectPool;

public class MS_ObjPool : ScriptBehaviour
{
    //Static list of MS_MarineSNow instances
    public static List<MS_MarineSnow> ms_Objs;
    
    public override void OnStart()
    {
        //Create a new list at runtime every time
        ms_Objs = new List<MS_MarineSnow>();
    }

    public override void OnUpdate()
    {
        // Called every frame
        if (Input.IsKeyPressed(KeyCode.P))
        {
            foreach (MS_MarineSnow obj in ms_Objs)
            {
                Log($"MS_Item: {obj.objID} detected!============");

                Entity fromIdEntity = Entity.FromId(obj.objID);
                //Declare and assign a new reference to the according Obj thru ID
                ref LocalTransform transform3 = ref fromIdEntity.GetComponent<LocalTransform>();
                Log("Entity FromId(): " + transform3.Position);
            }
        }
        //if (Input.IsKeyPressed(KeyCode.L))
        //{
        //    foreach (MS_MarineSnow obj in ms_Objs)
        //    {
        //        Entity fromIdEntity = Entity.FromId(obj.objID);
        //        //Declare and assign a new reference to the according Obj thru ID
        //        ref Active active = ref fromIdEntity.GetComponent<Active>();
        //        //Get the relavant ref component u need 
        //        active.Enabled = !active.Enabled;
        //        //toggle enabled
        //    }
        //}
    }
}

