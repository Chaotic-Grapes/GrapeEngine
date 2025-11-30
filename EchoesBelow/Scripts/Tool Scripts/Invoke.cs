using GrapeEngine.Scripting;

namespace Scripts.Tool_Scripts;

public class Invoke : ScriptBehaviour
{
    //this might not work Try again another time!
    private float timer;
    private float delay { get; set; }
    bool targetBool, setBool;
    float targetFloat, setFloat;
    int targetInt, setInt;
    string targetString, setString;
    public Invoke(ref bool targetBool, bool setBool, float delay)
    {
        this.targetBool = targetBool;
        this.setBool = setBool;
        this.delay = delay;
    }
    public Invoke(ref float targetFloat, float setFloat, float delay)
    {
        this.targetFloat = targetFloat;
        this.setFloat = setFloat;
        this.delay = delay;
    }
    public Invoke(ref int targetInt, int setInt, float delay)
    {
        this.targetInt = targetInt;
        this.setInt = setInt;
        this.delay = delay;
    }
    public Invoke(ref string targetString, string setString, float delay)
    {
        this.targetString = targetString;
        this.setString = setString;
        this.delay = delay;
    }
    //this script was created to address 
    public override void OnStart()
    {
        timer = 0f;
    }

    public override void OnUpdate()
    {
        if (timer <= -1) return;
        timer += Time.UnscaledDeltaTime;
        Log("GO!");
        if (timer > delay)
        {
            SetTargets();
            
        }
        
        // Called every frame
    }
    private void SetTargets()
    {
        targetBool = setBool;
        targetFloat = setFloat;
        targetInt = setInt;
        targetString = setString;

        timer = -1;
    }
}

