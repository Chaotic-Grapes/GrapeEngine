using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace TestGame;

/// <summary>
/// Player controller script - demonstrates keyboard input and movement.
/// This is a unique behavior that controls the player entity.
/// </summary>
public class PlayerController : ScriptBehaviour
{
    // Movement settings
    private readonly float m_moveSpeed = 200.0f;
    private float m_rotationSpeed = 180.0f;

    // World bounds
    private const float WorldWidth = 1280.0f;
    private const float WorldHeight = 720.0f;
    private const float BoundaryMargin = 30.0f;

    public override void OnStart()
    {
        Log("PlayerController initialized!", LogLevel.Info);
        Log("Use WASD to move, Q/E to rotate", LogLevel.Info);
    }

    public override void OnUpdate()
    {
        if (!TryGetComponent<LocalTransform>(out var transform))
        {
            Log("PlayerController: No LocalTransform component!", LogLevel.Warning);
            return;
        }

        // Calculate movement based on input
        var movement = Vector3.Zero;

        // WASD movement (for now we'll use Time-based auto-movement as placeholder)
        // When Input API is available, replace this with actual input
        
        // Demo movement pattern - move in a figure-8
        var t = (float)Time.ElapsedTime * 0.5f;
        var targetX = WorldWidth * 0.5f + MathF.Cos(t) * 300.0f;
        var targetY = WorldHeight * 0.5f + MathF.Sin(t * 2.0f) * 150.0f;
        
        // Smooth movement towards target
        var target = new Vector3(targetX, targetY, 0.0f);
        var direction = target - transform.Position;
        
        if (direction.Magnitude > 5.0f)
        {
            movement = direction.Normalized * m_moveSpeed * Time.DeltaTime;
        }

        // Apply movement
        transform.Position += movement;

        // Clamp to world bounds
        transform.Position.X = Math.Clamp(transform.Position.X, BoundaryMargin, WorldWidth - BoundaryMargin);
        transform.Position.Y = Math.Clamp(transform.Position.Y, BoundaryMargin, WorldHeight - BoundaryMargin);

        // Update component
        SetComponent(transform);

        // Update visual feedback
        UpdateVisual();
    }

    private void UpdateVisual()
    {
        // Make the player pulse to show it's active
        if (!TryGetComponent<ShapeCircle2D>(out var circle))
            return;

        var pulse = 0.9f + 0.1f * MathF.Sin((float)Time.ElapsedTime * 5.0f);
        circle.Radius = 20.0f * pulse;
            
        // Keep green color but vary brightness
        circle.Color.R = 0;
        circle.Color.G = (byte)(255 * pulse);
        circle.Color.B = 0;
        circle.Color.A = 255;
            
        SetComponent(circle);
    }

    public override void OnFixedUpdate()
    {
        // Physics-based updates would go here if needed
    }
}
