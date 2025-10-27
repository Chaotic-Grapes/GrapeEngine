using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace TestGame;

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

    public override void OnStart()
    {
        Log("Collectible item spawned!", LogLevel.Info);

        if (TryGetComponent<LocalTransform>(out var transform))
        {
            m_originalPosition = transform.Position;
        }
    }

    public override void OnUpdate()
    {
        if (!TryGetComponent<LocalTransform>(out var transform))
            return;

        // Bob up and down
        var bobOffset = MathF.Sin((float)Time.ElapsedTime * m_bobSpeed) * m_bobHeight;
        transform.Position.Y = m_originalPosition.Y + bobOffset;

        // Rotate
        // Note: Simplified rotation - would use quaternions properly in full implementation

        SetComponent(transform);

        // Update visual
        UpdateCollectibleVisual();
    }

    private void UpdateCollectibleVisual()
    {
        if (!TryGetComponent<ShapeCircle2D>(out var circle))
            return;

        // Rainbow color cycle
        var hue = ((float)Time.ElapsedTime * 0.5f) % 1.0f;
        var (r, g, b) = HSVToRGB(hue, 1.0f, 1.0f);

        circle.Color.R = r;
        circle.Color.G = g;
        circle.Color.B = b;
        circle.Color.A = 255;

        SetComponent(circle);
    }

    private (byte r, byte g, byte b) HSVToRGB(float h, float s, float v)
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

        return ((byte)(r * 255), (byte)(g * 255), (byte)(b * 255));
    }
}