using GrapeEngine.ScriptAPI;

namespace MyGame
{
    /// <summary>
    /// Bouncy ball that changes color based on velocity
    /// Demonstrates: Physics API, Math API, Component access
    /// </summary>
    public class BouncyBall : ScriptBehavior
    {
        private float colorChangeSpeed = 2.0f;
        private float hue = 0.0f;

        public override void OnStart()
        {
            var transform = GetComponent<LocalTransform>(EntityId);
            Debug.Log($"BouncyBall spawned at ({transform.Position.X:F2}, {transform.Position.Y:F2})");
            
            // Set random initial hue
            hue = Math.RandomFloat(0.0f, 360.0f);
        }

        public override void OnUpdate()
        {
            // Get velocity to determine color intensity
            var velocity = Physics.GetVelocity(EntityId);
            float speed = Math.Length2D(velocity.X, velocity.Y);
            
            // Change hue over time
            hue += colorChangeSpeed * Time.GetDeltaTime();
            if (hue > 360.0f)
                hue -= 360.0f;

            // Map speed to brightness (demonstrates clamping)
            float brightness = Math.Clamp(speed * 0.1f, 0.3f, 1.0f);

            // Simple HSV to RGB conversion for demo purposes
            // Note: In real implementation, you'd set the ShapeCircle2D component color
            float normalizedHue = hue / 60.0f;
            int region = (int)Math.Floor(normalizedHue);
            float fraction = normalizedHue - region;

            // Log occasionally for debugging
            if (Math.RandomFloat(0.0f, 1.0f) < 0.01f) // 1% chance per frame
            {
                Debug.Log($"Ball speed: {speed:F2}, brightness: {brightness:F2}, hue: {hue:F0}");
            }
        }
    }
}
