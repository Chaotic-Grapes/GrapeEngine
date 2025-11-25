using GrapeEngine.Scripting;


namespace Scripts;

public class UIClickableTest : ScriptBehaviour
{
    Color startCol;
    public override void OnStart()
    {
        // Called once when the script is initialized
        ref UIClickable click = ref GetComponent<UIClickable>();
        ref SpriteRenderer2D sr = ref GetComponent<SpriteRenderer2D>();

        startCol = sr.Color;
        Log($"startCol: R/{startCol.R} G/{startCol.G} B/{startCol.B} A/{startCol.A}");
    }

    public override void OnUpdate()
    {
        // Called every frame
        ref UIClickable click = ref GetComponent<UIClickable>();
        ref SpriteRenderer2D sr = ref GetComponent<SpriteRenderer2D>();

        if (click.IsPressed)
        {
            sr.Color = new Color(1,0,0,1);
        }
        else if (click.IsHovered)
        {
            sr.Color = new Color(0, 1, 0, 1);
        }
        else
        {
            sr.Color = startCol;
        }
        //Log("UIClickable ID: " + click.ClickActionID);


    }
}

