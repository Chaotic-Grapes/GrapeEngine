using GrapeEngine.ScriptAPI;

namespace MyGame
{
    /// <summary>
    /// Player controller that responds to keyboard input and applies forces
    /// Demonstrates: Input API, Physics API, Math API, Debug API
    /// </summary>
    public class PlayerController : ScriptBehavior
    {
        private float moveForce = 150.0f;
        private float jumpForce = 300.0f;
        private bool isGrounded = false;

        public override void OnStart()
        {
            Debug.Log($"PlayerController started on entity {EntityId}");
            Debug.Log($"Window size: {Window.GetWidth()}x{Window.GetHeight()}");
            Debug.Log($"Physics gravity: {Physics.GetGravity()}");
        }

        public override void OnUpdate()
        {
            // Check if grounded (simple ground check based on Y position)
            var transform = GetComponent<LocalTransform>(EntityId);
            isGrounded = transform.Position.Y < -1.5f;

            // Horizontal movement with WASD or Arrow keys
            float moveX = 0.0f;
            
            if (Input.IsKeyDown(KeyCode.A) || Input.IsKeyDown(KeyCode.Left))
            {
                moveX -= 1.0f;
            }
            if (Input.IsKeyDown(KeyCode.D) || Input.IsKeyDown(KeyCode.Right))
            {
                moveX += 1.0f;
            }

            // Apply horizontal force
            if (moveX != 0.0f)
            {
                Physics.ApplyForce(EntityId, moveX * moveForce, 0.0f);
            }

            // Jump with Space or W/Up
            if (isGrounded && (Input.IsKeyPressed(KeyCode.Space) || 
                               Input.IsKeyPressed(KeyCode.W) || 
                               Input.IsKeyPressed(KeyCode.Up)))
            {
                Physics.ApplyImpulse(EntityId, 0.0f, jumpForce);
                Debug.Log("Player jumped!");
            }

            // Get current velocity for debug
            var velocity = Physics.GetVelocity(EntityId);
            
            // Random color change on mouse click (demonstrates Math API)
            if (Input.IsMousePressed(MouseButton.Left))
            {
                float r = Math.RandomFloat(0.0f, 1.0f);
                float g = Math.RandomFloat(0.0f, 1.0f);
                float b = Math.RandomFloat(0.0f, 1.0f);
                
                // Note: SetComponent not yet implemented for ShapeCircle2D color
                Debug.Log($"Random color generated: R={r:F2}, G={g:F2}, B={b:F2}");
            }

            // Display info on key press
            if (Input.IsKeyPressed(KeyCode.I))
            {
                Debug.Log($"=== Player Info ===");
                Debug.Log($"Position: ({transform.Position.X:F2}, {transform.Position.Y:F2})");
                Debug.Log($"Velocity: ({velocity.X:F2}, {velocity.Y:F2})");
                Debug.Log($"Speed: {Math.Length2D(velocity.X, velocity.Y):F2}");
                Debug.Log($"Grounded: {isGrounded}");
                Debug.Log($"Time Scale: {Time.GetTimeScale():F2}");
                Debug.Log($"FPS: {(1.0f / Time.GetDeltaTime()):F0}");
            }

            // Time scale control
            if (Input.IsKeyPressed(KeyCode.Num1))
            {
                Time.SetTimeScale(0.5f);
                Debug.Log("Time scale: 0.5x (slow motion)");
            }
            if (Input.IsKeyPressed(KeyCode.Num2))
            {
                Time.SetTimeScale(1.0f);
                Debug.Log("Time scale: 1.0x (normal)");
            }
            if (Input.IsKeyPressed(KeyCode.Num3))
            {
                Time.SetTimeScale(2.0f);
                Debug.Log("Time scale: 2.0x (fast forward)");
            }

            // Quit on Escape
            if (Input.IsKeyPressed(KeyCode.Escape))
            {
                Debug.LogWarning("Quitting application...");
                Application.Quit();
            }
        }
    }
}
