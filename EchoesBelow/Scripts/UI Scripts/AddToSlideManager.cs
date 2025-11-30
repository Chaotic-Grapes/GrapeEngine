using GrapeEngine.Scripting;

namespace Scripts.UI_Scripts;

public class AddToSlideManager : ScriptBehaviour
{
    public override void OnStart()
    {
        // Called once when the script is initialized
        UI_SlideManager.slides.Add(this.Entity.EntityId);
        Log("Added to SlideManager");
    }

}

