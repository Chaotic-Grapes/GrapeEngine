using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

/// <summary>
/// Collectible item to demonstrate another unique behavior.
/// </summary>
public class CollectibleItem : ScriptBehaviour
{
    private readonly float m_bobSpeed = 2.0f;
    private readonly float m_bobHeight = 10.0f;
    private Vector3 m_originalPosition;

    // Visual entity
    private Entity m_visualEntity;

    public override void OnStart()
    {
        Log("Collectible item spawned!", LogLevel.Info);

        // Create the visual entity
        m_visualEntity = CreateEntity(
            new ComponentData<LocalTransform>(new()
            {
                Position = Vector3.Zero,
                Rotation = Quaternion.Identity,
                Scale = new Vector3(1, 1, 1)
            }),
            new ComponentData<ShapeCircle2D>(new()
            {
                Radius = 12.0f,
                Color = new Color { R = 1.0f, G = 0.84f, B = 0.0f, A = 1.0f }, // Gold color
                Filled = true
            }),
            new ComponentData<Layer>(new() { Id = 0 }),
            new ComponentData<Active>(new() { Enabled = true })
        );

        Log($"Collectible visual entity created: {m_visualEntity.EntityId}", LogLevel.Info);
    }

    public override void OnUpdate()
    {
        ref var visualTransform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var Transform);
        if (!Transform) return;

        // Bob up and down
        var bobOffset = MathF.Sin((float)Time.ElapsedTime * m_bobSpeed) * m_bobHeight;
        visualTransform.Position.Y = m_originalPosition.Y + bobOffset;

        // Rotate
        // Note: Simplified rotation - would use quaternions properly in full implementation

        // Update visual
        UpdateCollectibleVisual();
    }

    private void UpdateCollectibleVisual()
    {
        ref var shapeVisual = ref m_visualEntity.TryGetComponent<ShapeCircle2D>(out var circle);
        if (!circle) return;

        // Rainbow color cycle
        var hue = ((float)Time.ElapsedTime * 0.5f) % 1.0f;
        var (r, g, b) = HSVToRGB(hue, 1.0f, 1.0f);
        // Since hue is being used, we need to convert HSV to RGB
        // More details below in HSVToRGB method

        shapeVisual.Color.R = r;
        shapeVisual.Color.G = g;
        shapeVisual.Color.B = b;
        shapeVisual.Color.A = 1.0f;
    }

    // Note: This is only a sample method for the sake of the demo.
    // Method adapted from https://gist.github.com/mjackson/5311256
    private (float r, float g, float b) HSVToRGB(float h, float s, float v)
    {
        var hi = (int)(h * 6.0f) % 6;
        var f = h * 6.0f - hi;
        var p = v * (1.0f - s);
        var q = v * (1.0f - f * s);
        var t = v * (1.0f - (1.0f - f) * s);

        float r, g, b;
        switch (hi)
        {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }

        return (r, g, b);
    }
}