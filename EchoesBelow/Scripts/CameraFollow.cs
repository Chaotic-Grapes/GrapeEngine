using GrapeEngine;
using GrapeEngine.Math;
using GrapeEngine.Numerics;
using GrapeEngine.Scripting;

namespace EchoesBelow.Scripts;

public class CameraFollow : ScriptBehaviour
{
    private const float SmoothSpeed = 5.0f;
    private const float OffsetX = 0.0f;
    private const float OffsetY = 0.0f;

    private Entity? _targetEntity;

    public override void OnStart()
    {
        // Find the player entity by name
        Log("CameraFollow: Searching for player...", LogLevel.Info);
    }

    public override void OnUpdate()
    {
        // Try to find the player if we haven't found it yet
        if (_targetEntity == null)
        {
            return;
        }

        // Get player's transform
        if (!_targetEntity.HasComponent<LocalTransform>())
            return;

        var playerTransform = _targetEntity.GetComponent<LocalTransform>();
        
        // Get camera's transform
        ref var cameraTransform = ref GetComponent<LocalTransform>();

        // Calculate target position with offset
        var targetX = playerTransform.Position.X + OffsetX;
        var targetY = playerTransform.Position.Y + OffsetY;

        // Smoothly interpolate to target position
        var smoothX = GMath.Lerp(cameraTransform.Position.X, targetX, SmoothSpeed * Time.DeltaTime);
        var smoothY = GMath.Lerp(cameraTransform.Position.Y, targetY, SmoothSpeed * Time.DeltaTime);

        // Update camera position (keep Z the same)
        cameraTransform.Position = new Vector3(smoothX, smoothY, cameraTransform.Position.Z);
    }

    public void SetTarget(Entity target)
    {
        _targetEntity = target;
        Log($"CameraFollow: Target set to entity {target.EntityId}", LogLevel.Info);
    }
}
