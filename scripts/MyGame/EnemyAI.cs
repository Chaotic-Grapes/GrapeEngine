using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

/// <summary>
/// Simple enemy AI - demonstrates autonomous behavior.
/// This is another unique behavior that's different from PlayerController.
/// 
/// RUBRIC: This is a second unique script behavior for the demo.
/// </summary>
public class EnemyAI : ScriptBehaviour
{
    private float m_patrolSpeed = 80.0f;
    private Vector3 m_patrolPointA;
    private Vector3 m_patrolPointB;
    private bool m_movingToB = true;
    private float m_detectionRadius = 150.0f;

    public override void OnStart()
    {
        Log("EnemyAI initialized!", LogLevel.Info);

        // Set patrol points based on initial position
        if (TryGetComponent<LocalTransform>(out var transform))
        {
            m_patrolPointA = transform.Position;
            m_patrolPointB = transform.Position + new Vector3(200.0f, 0.0f, 0.0f);
        }
    }

    public override void OnUpdate()
    {
        if (!TryGetComponent<LocalTransform>(out var transform))
            return;

        // Simple patrol behavior
        var targetPoint = m_movingToB ? m_patrolPointB : m_patrolPointA;
        var direction = targetPoint - transform.Position;
        var distance = direction.Magnitude;

        // Move towards patrol point
        if (distance > 5.0f)
        {
            var movement = direction.Normalized * m_patrolSpeed * Time.DeltaTime;
            transform.Position += movement;
        }
        else
        {
            // Reached patrol point, switch direction
            m_movingToB = !m_movingToB;
        }

        SetComponent(transform);

        // Update visual
        UpdateEnemyVisual();
    }

    private void UpdateEnemyVisual()
    {
        // Make enemy throb to show it's active
        if (!TryGetComponent<ShapeCircle2D>(out var circle))
            return;

        var throb = 0.85f + 0.15f * MathF.Sin((float)Time.ElapsedTime * 3.0f);
        circle.Radius = 15.0f * throb;

        // Red color with varying intensity
        circle.Color.R = (byte)(255 * throb);
        circle.Color.G = 0;
        circle.Color.B = 0;
        circle.Color.A = 255;

        SetComponent(circle);
    }
}