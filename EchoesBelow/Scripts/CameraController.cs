using GrapeEngine;
using GrapeEngine.Math;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace Scripts;

public class CameraController : ScriptBehaviour
{
    private const float SmoothSpeed = 5.0f;
    private const float OffsetX = 0.0f;
    private const float OffsetY = 0.0f;

    private Entity? _targetEntity;
    public override void OnUpdate()
    {
        // Called every frame
       
        //Get player's position
        var playerTransform = Player.playerPos;

        // Get camera's transform
        ref var cameraTransform = ref GetComponent<LocalTransform>();

        // Calculate target position with offset
        var targetX = playerTransform.X + OffsetX;
        var targetY = playerTransform.Y + OffsetY;

        // Smoothly interpolate to target position
        var smoothX = GMath.Lerp(cameraTransform.Position.X, targetX, SmoothSpeed * Time.DeltaTime);
        var smoothY = GMath.Lerp(cameraTransform.Position.Y, targetY, SmoothSpeed * Time.DeltaTime);

        // Update camera position (keep Z the same)
        cameraTransform.Position = new Vector3(smoothX, smoothY, cameraTransform.Position.Z);
    }
}

