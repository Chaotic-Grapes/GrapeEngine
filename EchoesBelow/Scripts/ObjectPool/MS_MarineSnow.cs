using GrapeEngine.Scripting;

namespace Scripts.ObjectPool;

public class MS_MarineSnow : ScriptBehaviour
{
    //accessible fields
    public ulong objID;
    public MS_MarineSnow instance;

    public override void OnStart()
    {
        objID = this.Entity.EntityId;
        instance = this;
        // assign this field
        MS_ObjPool.ms_Objs.Add(this);

    }
}

