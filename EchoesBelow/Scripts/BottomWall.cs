using GrapeEngine.Scripting;
using GrapeEngine.Math;
using GrapeEngine.Physics;
using GrapeEngine.Events;

namespace Scripts;

/// <summary>
/// Bottom wall that turns blue when Player collides with it.
/// </summary>
public class BottomWall : ScriptBehaviour
{
    // Colors
    private Color originalColor = new Color(1.0f, 1.0f, 1.0f, 1.0f);
    private Color collisionColor = new Color(0.0f, 0.5f, 1.0f, 1.0f);
    private Color currentColor;
    
    // Collision state
    private bool isColliding = false;
    
    // Transition speed
    private const float colorTransitionSpeed = 5.0f;

    public override void OnStart()
    {
        ref ShapeBox2D box = ref GetComponent<ShapeBox2D>();
        originalColor = box.Color;
        currentColor = originalColor;
        
        Log("BottomWall ready - waiting for Player...");
    }

    public override void OnUpdate()
    {
        // Check if Player has registered itself yet
        if (Player.playerEntityId == 0)
        {
            // Player hasn't started yet, skip this frame
            return;
        }
        
        // Get collision events for this wall
        var events = CollisionEvents.GetEvents(Entity);
        
        bool playerIsColliding = false;
        
        foreach (var evt in events)
        {
            // Check if the collision is with the Player
            if (evt.Other.EntityId == Player.playerEntityId)
            {
                if (evt.Type == CollisionEventType.Enter)
                {
                    Log("PLAYER collision ENTER!");
                    playerIsColliding = true;
                }
                else if (evt.Type == CollisionEventType.Stay)
                {
                    playerIsColliding = true;
                }
                else if (evt.Type == CollisionEventType.Exit)
                {
                    Log("PLAYER collision EXIT!");
                }
            }
        }
        
        // Update collision state
        isColliding = playerIsColliding;
        
        // Smooth color transition
        Color targetColor = isColliding ? collisionColor : originalColor;
        
        currentColor.R = GMath.Lerp(currentColor.R, targetColor.R, colorTransitionSpeed * Time.DeltaTime);
        currentColor.G = GMath.Lerp(currentColor.G, targetColor.G, colorTransitionSpeed * Time.DeltaTime);
        currentColor.B = GMath.Lerp(currentColor.B, targetColor.B, colorTransitionSpeed * Time.DeltaTime);
        currentColor.A = GMath.Lerp(currentColor.A, targetColor.A, colorTransitionSpeed * Time.DeltaTime);
        
        // Apply color to box
        ref ShapeBox2D box = ref GetComponent<ShapeBox2D>();
        box.Color = currentColor;
    }
}