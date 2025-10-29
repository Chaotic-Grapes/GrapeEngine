using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

/// <summary>
/// Simple enemy AI to demonstrate autonomous behavior.
/// </summary>
public class EnemyAI : ScriptBehaviour
{
    //AI state machine
    private HFSM m_fsm;

    // stats
    private readonly float m_patrolSpeed = 80f;
    private Vector3 m_patrolPointA;
    private Vector3 m_patrolPointB;
    private bool m_movingToB = true;
    private float m_detectionRadius = 150f;

    // Visual entity
    private Entity m_visualEntity;

    public override void OnStart()
    {
        Log("EnemyAI initialized!", LogLevel.Info);

        // Create the visual entity for this enemy
        m_visualEntity = CreateEntity(
            new ComponentData<LocalTransform>(new()
            {
                Position = new Vector3(100f, 100f, 0f),
                Rotation = Quaternion.Identity,
                Scale = new Vector3(1, 1, 1)
            }),
            // Add red circle visual
            new ComponentData<ShapeCircle2D>(new()
            {
                Radius = 15.0f,
                Color = new Color { R = 1f, G = 0f, B = 0f, A = 1f },
                Filled = true
            }),
            new ComponentData<Layer>(new() { Id = 0 }),
            new ComponentData<Active>(new() { Enabled = true })
        );

        var transform = m_visualEntity.GetComponent<LocalTransform>();

        // Set patrol points
        m_patrolPointA = transform.Position;
        m_patrolPointB = transform.Position + new Vector3(200.0f, 0.0f, 0.0f);

        Log($"Enemy visual entity created: {m_visualEntity.EntityId}", LogLevel.Info);
    }

    public override void OnUpdate()
    {
        if (!m_visualEntity.TryGetComponent<LocalTransform>(out var transform))
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

        m_visualEntity.SetComponent(transform);

        // Update visual
        UpdateEnemyVisual();
    }

    private void UpdateEnemyVisual()
    {
        // Make enemy throb to show it's active
        if (!m_visualEntity.TryGetComponent<ShapeCircle2D>(out var circle))
            return;

        var throb = 0.85f + 0.15f * MathF.Sin((float)Time.ElapsedTime * 3.0f);
        circle.Radius = 15.0f * throb;

        // Red color with varying intensity
        circle.Color.R = throb;
        circle.Color.G = 0.0f;
        circle.Color.B = 0.0f;
        circle.Color.A = 1.0f;

        m_visualEntity.SetComponent(circle);
    }
}