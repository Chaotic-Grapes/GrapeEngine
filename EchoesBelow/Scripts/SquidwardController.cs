using System;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public sealed class SquidwardController : SystemBase
{
    private const string SquidwardName = "squidward";
    private const float MoveSpeed = 2.5f;

    protected override void OnUpdate()
    {
        if (World == null)
            return;

        if (!TryFindEntityByName(World, SquidwardName, out var squidward))
            return;

        ApplyMovement(squidward);
    }

    private void ApplyMovement(Entity entity)
    {
        if (!entity.IsAlive || !entity.HasComponent<LocalTransform>())
            return;

        float x = 0.0f;
        float y = 0.0f;

        if (Input.IsKeyDown(KeyCode.A)) x -= 1.0f;
        if (Input.IsKeyDown(KeyCode.D)) x += 1.0f;
        if (Input.IsKeyDown(KeyCode.W)) y += 1.0f;
        if (Input.IsKeyDown(KeyCode.S)) y -= 1.0f;

        if (x == 0.0f && y == 0.0f)
            return;

        float length = (float)Math.Sqrt((x * x) + (y * y));
        if (length > 0.0f)
        {
            x /= length;
            y /= length;
        }

        ref var transform = ref entity.GetComponent<LocalTransform>();
        var position = transform.Position;
        position.X += x * MoveSpeed * Time.DeltaTime;
        position.Y += y * MoveSpeed * Time.DeltaTime;
        transform.Position = position;
    }

    private static bool TryFindEntityByName(World world, string name, out Entity entity)
    {
        entity = default;
        foreach (var result in world.Query<Name>())
        {
            var candidate = result.Entity;
            string candidateName = Strings.Resolve(result.Component1.Value) ?? string.Empty;
            if (string.Equals(candidateName, name, StringComparison.OrdinalIgnoreCase))
            {
                entity = candidate;
                return true;
            }
        }

        return false;
    }
}
