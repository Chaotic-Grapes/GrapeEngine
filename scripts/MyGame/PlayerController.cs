using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

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

    // Visual entity that represents the player
    private Entity m_visualEntity;

    public override void OnStart()
    {
        Log("PlayerController initialized!", LogLevel.Info);
        Log("Use WASD to move, Q/E to rotate", LogLevel.Info);

        // Create the visual entity for the player
        m_visualEntity = CreateEntity();
        
        // Set up initial position at center of screen
        var transform = new LocalTransform
        {
            Position = new Vector3(WorldWidth * 0.5f, WorldHeight * 0.5f, 0.0f),
            Rotation = Quaternion.Identity,
            Scale = new Vector3(1, 1, 1)
        };
        m_visualEntity.SetComponent(transform);

        // Add green circle visual
        var circle = new ShapeCircle2D
        {
            Radius = 20.0f,
            Color = new Color { R = 0.0f, G = 1.0f, B = 0.0f, A = 1.0f },
            Filled = true
        };
        m_visualEntity.SetComponent(circle);

        // Make sure it's active
        m_visualEntity.SetComponent(new Active { Enabled = true });

        Log($"Player visual entity created: {m_visualEntity.EntityId}", LogLevel.Info);
    }

    public override void OnUpdate()
    {
        if (!m_visualEntity.TryGetComponent<LocalTransform>(out var transform))
        {
            Log("PlayerController: No LocalTransform component on visual entity!", LogLevel.Warning);
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
        m_visualEntity.SetComponent(transform);

        // Update visual feedback
        UpdateVisual();
    }

    private void UpdateVisual()
    {
        // Make the player pulse to show it's active
        if (!m_visualEntity.TryGetComponent<ShapeCircle2D>(out var circle))
            return;

        var pulse = 0.9f + 0.1f * MathF.Sin((float)Time.ElapsedTime * 5.0f);
        circle.Radius = 20.0f * pulse;
            
        // Keep green color but vary brightness
        circle.Color.R = 0.0f;
        circle.Color.G = pulse;
        circle.Color.B = 0.0f;
        circle.Color.A = 1.0f;
            
        m_visualEntity.SetComponent(circle);
    }

    public override void OnFixedUpdate()
    {
        // Physics-based updates would go here if needed
    }
}
