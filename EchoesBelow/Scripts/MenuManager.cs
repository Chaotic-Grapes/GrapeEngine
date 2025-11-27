using GrapeEngine.Scripting;

namespace Scripts;

public class MenuManager : ScriptBehaviour
{
    //public bool isMenuManager = true;   
    //bool isPaused = false;
    //bool selectingResume = true;        // true = resume, false = exit

    //// Cached references
    //UIClickable resumeClick;
    //UIClickable exitClick;
    //SpriteRenderer2D resumeSR;
    //SpriteRenderer2D exitSR;

    //public override void OnStart()
    //{
    //    // Find children by name
    //    var resumeObj = FindChild("ResumeButton");
    //    var exitObj = FindChild("ExitButton");

    //    if (Entity.Name == "Menu")
    //        isMenuManager = true;

    //    // Get components
    //    ref resumeClick = resumeObj.GetComponent<UIClickable>();
    //    ref exitClick = exitObj.GetComponent<UIClickable>();

    //    ref resumeSR = resumeObj.GetComponent<SpriteRenderer2D>();
    //    ref exitSR = exitObj.GetComponent<SpriteRenderer2D>();

    //    // Hide menu
    //    entity.Active = false;
    //}

    //public override void OnUpdate()
    //{
    //    if (!isMenuManager) return;

    //    // Toggle menu
    //    if (Input.IsKeyPressed(KeyCode.Escape))
    //    {
    //        isPaused = true;
    //        entity.Active = true;
    //    }

    //    if (!isPaused) return;

    //    // Tab switching
    //    if (Input.IsKeyPressed(KeyCode.Tab))
    //    {
    //        selectingResume = !selectingResume;
    //    }

    //    UpdateHighlight();

    //    // If user clicks a button
    //    if (selectingResume && resumeClick.IsPressed)
    //        ResumeGame();

    //    if (!selectingResume && exitClick.IsPressed)
    //        ExitGame();
    //}

    //void UpdateHighlight()
    //{
    //    if (selectingResume)
    //    {
    //        resumeSR.Color = new Color(0, 1, 0, 1);
    //        exitSR.Color = new Color(1, 1, 1, 1);
    //    }
    //    else
    //    {
    //        resumeSR.Color = new Color(1, 1, 1, 1);
    //        exitSR.Color = new Color(0, 1, 0, 1);
    //    }
    //}

    //void ResumeGame()
    //{
    //    isPaused = false;
    //    entity.Active = false;
    //}

    //void ExitGame()
    //{
    //    Engine.Application.Quit();
    //}
}
