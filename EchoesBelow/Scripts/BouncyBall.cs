using GrapeEngine.Math;
using GrapeEngine.Physics;
using GrapeEngine.Scripting;

namespace EchoesBelow.Scripts;

/// <summary>
/// Bouncy ball that changes color based on velocity
/// Demonstrates: Physics API, Math API, Component access
/// </summary>
public class BouncyBall : ScriptBehaviour
{
    private const float ColorChangeSpeed = 2.0f;
    private float _hue;

    public override void OnStart()
    {
        var transform = GetComponent<LocalTransform>();
        Log($"BouncyBall spawned at ({transform.Position.X:F2}, {transform.Position.Y:F2})");
        
        // Set random initial hue
        _hue = GMath.RandomFloat(0.0f, 360.0f);
    }

    public override void OnUpdate()
    {
        // Get velocity to determine color intensity
        var velocity = Physics.GetVelocity(Entity);
        var speed = GMath.Length(velocity);
        
        // Change hue over time
        _hue += ColorChangeSpeed * Time.DeltaTime;
        if (_hue > 360.0f)
            _hue -= 360.0f;

        // Map speed to brightness (demonstrates clamping)
        var brightness = GMath.Clamp(speed * 0.1f, 0.3f, 1.0f);

        // Simple HSV to RGB conversion for demo purposes
        // Note: In real implementation, you'd set the ShapeCircle2D component color
        var normalizedHue = _hue / 60.0f;
        var region = (int)GMath.Floor(normalizedHue);
        var fraction = normalizedHue - region;

        // Log occasionally for debugging
        if (GMath.RandomFloat(0.0f, 1.0f) < 0.01f) // 1% chance per frame
        {
            Log($"Ball speed: {speed:F2}, brightness: {brightness:F2}, hue: {_hue:F0}");
        }
    }
}
