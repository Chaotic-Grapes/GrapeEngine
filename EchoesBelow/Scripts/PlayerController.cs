using GrapeEngine.Numerics;
using GrapeEngine;
using GrapeEngine.Math;
using GrapeEngine.Physics;
using GrapeEngine.Scripting;

namespace EchoesBelow.Scripts;

/// <summary>
/// Player controller that responds to keyboard input and applies forces
/// Demonstrates: Input API, Physics API, Math API, Debug API
/// </summary>
public class PlayerController : ScriptBehaviour
{
    private const float MoveSpeed = 5.0f;
    private const float JumpVelocity = 8.0f;
    private bool _isGrounded;

    public override void OnStart()
    {
        Log($"PlayerController started on entity {EntityId}");
        Log($"Window size: {Window.Width}x{Window.Height}");
        Log($"Physics gravity: {Physics.Gravity}");
    }

    public override void OnUpdate()
    {
        // Check if grounded (simple ground check based on Y position)
        var transform = GetComponent<LocalTransform>();
        _isGrounded = transform.Position.Y <= -2.0f;

        // Get current velocity
        ref var velocity = ref GetComponent<LinearVelocity2D>();

        // Horizontal movement with WASD or Arrow keys
        var moveX = 0.0f;
        
        if (Input.IsKeyDown(KeyCode.A) || Input.IsKeyDown(KeyCode.Left))
        {
            moveX -= 1.0f;
        }
        if (Input.IsKeyDown(KeyCode.D) || Input.IsKeyDown(KeyCode.Right))
        {
            moveX += 1.0f;
        }

        // Set horizontal velocity directly
        velocity.Value.X = moveX * MoveSpeed;

        // Jump with Space or W/Up
        if (_isGrounded && (Input.IsKeyPressed(KeyCode.Space) || 
                           Input.IsKeyPressed(KeyCode.W) || 
                           Input.IsKeyPressed(KeyCode.Up)))
        {
            velocity.Value.Y = JumpVelocity;
            Log("Player jumped!");
        }

        // Random color change on mouse click (demonstrates Math API)
        if (Input.IsMousePressed(MouseButton.Left))
        {
            var r = GMath.RandomFloat(0.0f, 1.0f);
            var g = GMath.RandomFloat(0.0f, 1.0f);
            var b = GMath.RandomFloat(0.0f, 1.0f);
            
            // Note: SetComponent not yet implemented for ShapeCircle2D color
            Log($"Random color generated: R={r:F2}, G={g:F2}, B={b:F2}");
        }

        // Display info on key press
        if (Input.IsKeyPressed(KeyCode.I))
        {
            Log($"=== Player Info ===");
            Log($"Position: ({transform.Position.X:F2}, {transform.Position.Y:F2})");
            Log($"Velocity: ({velocity.Value.X:F2}, {velocity.Value.Y:F2})");
            Log($"Speed: {GMath.Length(velocity.Value):F2}");
            Log($"Grounded: {_isGrounded}");
            Log($"Time Scale: {Time.TimeScale:F2}");
            Log($"FPS: {(1f / Time.DeltaTime):F0}");
        }

        // Quit on Escape
        if (Input.IsKeyPressed(KeyCode.Escape))
        {
            Log("Quitting application...");
            Application.Quit();
        }
    }
}
