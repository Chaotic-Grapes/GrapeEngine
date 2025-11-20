using GrapeEngine.ScriptAPI;

namespace MyGame
{
    /// <summary>
    /// Object that continuously rotates and applies angular velocity
    /// Demonstrates: Time API, Math API, Rotation manipulation
    /// </summary>
    public class SpinningObject : ScriptBehavior
    {
        private float spinSpeed = 180.0f; // degrees per second
        private float wobbleAmount = 30.0f;
        private float wobbleFrequency = 2.0f;
        private float elapsedTime = 0.0f;

        public override void OnStart()
        {
            Debug.Log($"SpinningObject initialized on entity {EntityId}");
            
            // Set random initial spin direction
            if (Math.RandomFloat(0.0f, 1.0f) > 0.5f)
            {
                spinSpeed = -spinSpeed;
            }
        }

        public override void OnUpdate()
        {
            elapsedTime += Time.GetDeltaTime();

            // Calculate wobble using sine wave
            float wobbleAngle = Math.Sin(elapsedTime * wobbleFrequency) * wobbleAmount;
            
            // Calculate total rotation
            float rotation = spinSpeed * elapsedTime + wobbleAngle;
            
            // Convert to radians for the transform
            float rotationRad = Math.DegToRad(rotation);

            // Get current transform
            var transform = GetComponent<LocalTransform>(EntityId);
            
            // Update rotation (Quaternion from Z-axis rotation)
            // Note: In actual implementation, you would need to properly update the quaternion
            // This demonstrates the math conversion
            float halfAngle = rotationRad * 0.5f;
            transform.Rotation.W = Math.Cos(halfAngle);
            transform.Rotation.Z = Math.Sin(halfAngle);

            // Apply angular velocity to rigidbody
            // Calculate angular velocity as derivative of rotation
            float angularVel = Math.DegToRad(spinSpeed + wobbleAmount * wobbleFrequency * Math.Cos(elapsedTime * wobbleFrequency));
            
            // Note: SetComponent for AngularVelocity2D would be needed here
            // This is a demonstration of the calculation

            // Log rotation info occasionally
            if (Input.IsKeyPressed(KeyCode.R))
            {
                Debug.Log($"=== Spinning Object Info ===");
                Debug.Log($"Rotation: {rotation:F2} degrees ({rotationRad:F2} radians)");
                Debug.Log($"Wobble: {wobbleAngle:F2} degrees");
                Debug.Log($"Angular velocity: {angularVel:F2} rad/s");
                Debug.Log($"Time: {elapsedTime:F2}s");
            }

            // Reset on key press
            if (Input.IsKeyPressed(KeyCode.T))
            {
                elapsedTime = 0.0f;
                Debug.Log("Spinning object time reset!");
            }
        }
    }
}
