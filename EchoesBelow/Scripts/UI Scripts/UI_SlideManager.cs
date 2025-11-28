using GrapeEngine.Scripting;

namespace Scripts.UI_Scripts;

public class UI_SlideManager : ScriptBehaviour
{
    public static List<ulong> slides; 
    public override void OnStart()
    {
        // Called once when the script is initialized
        slides = new List<ulong>();
        Log("Slide list created!");
    }
    public override void OnUpdate()
    {
        if (Input.IsKeyPressed(KeyCode.P))
        {
            Log("P Pressed!");
            foreach (ulong entityId in slides)
            {
                Log($"slide: {entityId} detected!====");
            }
        }
    }
}

