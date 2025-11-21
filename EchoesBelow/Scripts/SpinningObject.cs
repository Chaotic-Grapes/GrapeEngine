using GrapeEngine.Math;
using GrapeEngine.Scripting;

namespace EchoesBelow.Scripts;

/// <summary>
/// Object that continuously rotates and applies angular velocity
/// Demonstrates: Time API, Math API, Rotation manipulation
/// </summary>
public class SpinningObject : ScriptBehaviour
{
    private float _spinSpeed = 180.0f; // degrees per second
    private const float WobbleAmount = 30.0f;
    private const float WobbleFrequency = 2.0f;
    private float _elapsedTime;

    public override void OnStart()
    {
        Log($"SpinningObject initialized on entity {EntityId}");
        
        // Set random initial spin direction
        if (GMath.RandomFloat(0.0f, 1.0f) > 0.5f)
        {
            _spinSpeed = -_spinSpeed;
        }
    }

    public override void OnUpdate()
    {
        _elapsedTime += Time.DeltaTime;

        // Calculate wobble using sine wave
        var wobbleAngle = GMath.Sin(_elapsedTime * WobbleFrequency) * WobbleAmount;
        
        // Calculate total rotation
        var rotation = _spinSpeed * _elapsedTime + wobbleAngle;
        
        // Convert to radians for the transform
        var rotationRad = GMath.DegToRad(rotation);

        // Get current transform
        ref var transform = ref GetComponent<LocalTransform>();
        
        // Update rotation (Quaternion from Z-axis rotation)
        // Note: In actual implementation, you would need to properly update the quaternion
        // This demonstrates the math conversion
        var halfAngle = rotationRad * 0.5f;
        transform.Rotation.W = GMath.Cos(halfAngle);
        transform.Rotation.Z = GMath.Sin(halfAngle);

        // Apply angular velocity to rigidbody
        // Calculate angular velocity as derivative of rotation
        var angularVel = GMath.DegToRad(_spinSpeed + WobbleAmount * WobbleFrequency * GMath.Cos(_elapsedTime * WobbleFrequency));
        
        // Note: SetComponent for AngularVelocity2D would be needed here
        // This is a demonstration of the calculation

        // Log rotation info occasionally
        if (Input.IsKeyPressed(KeyCode.R))
        {
            Log($"=== Spinning Object Info ===");
            Log($"Rotation: {rotation:F2} degrees ({rotationRad:F2} radians)");
            Log($"Wobble: {wobbleAngle:F2} degrees");
            Log($"Angular velocity: {angularVel:F2} rad/s");
            Log($"Time: {_elapsedTime:F2}s");
        }

        // Reset on key press
        if (Input.IsKeyPressed(KeyCode.T))
        {
            _elapsedTime = 0.0f;
            Log("Spinning object time reset!");
        }
    }
}
