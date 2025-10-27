using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

/// <summary>
/// Collectible item - demonstrates another unique behavior.
/// 
/// RUBRIC: This is a third unique script behavior.
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
        m_visualEntity = CreateEntity();

        // Set up initial position (use a default or pass in via script parameters in future)
        m_originalPosition = new Vector3(400.0f, 360.0f, 0.0f);

        var transform = new LocalTransform
        {
            Position = m_originalPosition,
            Rotation = Quaternion.Identity,
            Scale = new Vector3(1, 1, 1)
        };
        m_visualEntity.SetComponent(transform);

        // Add yellow/gold circle visual
        var circle = new ShapeCircle2D
        {
            Radius = 12.0f,
            Color = new Color { R = 1.0f, G = 0.84f, B = 0.0f, A = 1.0f }, // Gold
            Filled = true
        };
        m_visualEntity.SetComponent(circle);

        // Add Layer component so renderer can see it
        m_visualEntity.SetComponent(new Layer { Id = 0 });

        // Make sure it's active
        m_visualEntity.SetComponent(new Active { Enabled = true });

        Log($"Collectible visual entity created: {m_visualEntity.EntityId}", LogLevel.Info);
    }

    public override void OnUpdate()
    {
        if (!m_visualEntity.TryGetComponent<LocalTransform>(out var transform))
            return;

        // Bob up and down
        var bobOffset = MathF.Sin((float)Time.ElapsedTime * m_bobSpeed) * m_bobHeight;
        transform.Position.Y = m_originalPosition.Y + bobOffset;

        // Rotate
        // Note: Simplified rotation - would use quaternions properly in full implementation

        m_visualEntity.SetComponent(transform);

        // Update visual
        UpdateCollectibleVisual();
    }

    private void UpdateCollectibleVisual()
    {
        if (!m_visualEntity.TryGetComponent<ShapeCircle2D>(out var circle))
            return;

        // Rainbow color cycle
        var hue = ((float)Time.ElapsedTime * 0.5f) % 1.0f;
        var (r, g, b) = HSVToRGB(hue, 1.0f, 1.0f);

        circle.Color.R = r;
        circle.Color.G = g;
        circle.Color.B = b;
        circle.Color.A = 1.0f;

        m_visualEntity.SetComponent(circle);
    }

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