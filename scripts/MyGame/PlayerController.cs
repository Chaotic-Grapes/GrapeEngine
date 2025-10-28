using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

/// <summary>
/// Player controller script to demonstrate keyboard input and movement.
/// </summary>
public class PlayerController : ScriptBehaviour
{
    // Movement settings
    private readonly float m_moveSpeed = 200.0f;
    private float m_rotationSpeed = 180.0f;

    // Visual entity that represents the player
    private Entity m_visualEntity;

    public override void OnStart()
    {
        Log("PlayerController initialized!", LogLevel.Info);
        Log("Use WASD to move, Q/E to rotate", LogLevel.Info);

        // Create the visual entity for the player
        m_visualEntity = CreateEntity();
        
        // Set initial position at center of screen
        var transform = new LocalTransform
        {
            Position = new Vector3(World.Width * 0.5f, World.Height * 0.5f, 0.0f),
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

        // Add Layer component so renderer can see it
        m_visualEntity.SetComponent(new Layer { Id = 0 });

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

        // WASD movement with keyboard input
        var movement = Vector2.Zero;

        if (Input.IsKeyDown(KeyCode.W))
            movement.Y += 1.0f; // Up
        if (Input.IsKeyDown(KeyCode.S))
            movement.Y -= 1.0f; // Down
        if (Input.IsKeyDown(KeyCode.A))
            movement.X -= 1.0f; // Left
        if (Input.IsKeyDown(KeyCode.D))
            movement.X += 1.0f; // Right

        // Normalize diagonal movement so we don't move faster diagonally
        if (movement.Magnitude > 0.0f)
        {
            movement = movement.Normalized;
            transform.Position.X += movement.X * m_moveSpeed * Time.DeltaTime;
            transform.Position.Y += movement.Y * m_moveSpeed * Time.DeltaTime;
        }

        // Rotation with Q/E keys
        if (Input.IsKeyDown(KeyCode.Q))
        {
            // Rotate counter-clockwise (increase Z rotation)
            var angle = m_rotationSpeed * Time.DeltaTime * (MathF.PI / 180.0f);
            var currentAngle = 2.0f * MathF.Acos(transform.Rotation.W);
            var newAngle = currentAngle + angle;
            transform.Rotation.W = MathF.Cos(newAngle / 2.0f);
            transform.Rotation.Z = MathF.Sin(newAngle / 2.0f);
        }
        if (Input.IsKeyDown(KeyCode.E))
        {
            // Rotate clockwise (decrease Z rotation)
            var angle = m_rotationSpeed * Time.DeltaTime * (MathF.PI / 180.0f);
            var currentAngle = 2.0f * MathF.Acos(transform.Rotation.W);
            var newAngle = currentAngle - angle;
            transform.Rotation.W = MathF.Cos(newAngle / 2.0f);
            transform.Rotation.Z = MathF.Sin(newAngle / 2.0f);
        }

        // Clamp to world bounds
        transform.Position.X = Math.Clamp(transform.Position.X, World.WallThickness, World.Width - World.WallThickness);
        transform.Position.Y = Math.Clamp(transform.Position.Y, World.WallThickness, World.Height - World.WallThickness);

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
            
        // Keep green color but change brightness
        circle.Color.R = 0.0f;
        circle.Color.G = pulse;
        circle.Color.B = 0.0f;
        circle.Color.A = 1.0f;
            
        m_visualEntity.SetComponent(circle);
    }

    public override void OnFixedUpdate()
    {
        // Physics-based updates would go here if needed but not used in this demo
    }
}
