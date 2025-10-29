using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

public class EnemyAI : ScriptBehaviour
{
    //AI state machine
    private HFSM m_fsm;

    // stats
    public float BaseSpeed = 80f;
    public float Speed = 80f;
    private float m_detectionRadius = 150f;

    // incase we need it for specific game scenes
    public float Health = 100f;
    public float MaxHealth = 100f;

    // Visual entity of enemy
    private Entity m_visualEntity;

    // state references so it can be reusable
    private EnemyPatrolState m_patrolState;
    private EnemyChaseState m_chaseState;
    private EnemyAttackState m_attackState;

    // Player reference (need to read game manager probably info there)
    private Entity m_playerEntity;

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

        //var transform = m_visualEntity.GetComponent<LocalTransform>();

        //// Set patrol points
        //m_patrolPointA = transform.Position;
        //m_patrolPointB = transform.Position + new Vector3(200.0f, 0.0f, 0.0f);

        Log($"Enemy visual entity created: {m_visualEntity.EntityId}", LogLevel.Info);

        // build state machine
        BuildStateMachine();

        Log("Enemy ready with HFSM~!")
    }

    public override void OnUpdate()
    {
        //if (!m_visualEntity.TryGetComponent<LocalTransform>(out var transform))
        //    return;

        //// Simple patrol behavior
        //var targetPoint = m_movingToB ? m_patrolPointB : m_patrolPointA;
        //var direction = targetPoint - transform.Position;
        //var distance = direction.Magnitude;

        //// Move towards patrol point
        //if (distance > 5.0f)
        //{
        //    var movement = direction.Normalized * m_patrolSpeed * Time.DeltaTime;
        //    transform.Position += movement;
        //}
        //else
        //{
        //    // Reached patrol point, switch direction
        //    m_movingToB = !m_movingToB;
        //}

        //m_visualEntity.SetComponent(transform);



        // Update the state machine - basically runs the ai states
        m_fsm.Update(Time.DeltaTime);

        // Update visuals - enemy visuals
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

    private void BuildStateMachine()
    {
        //create all states 
        m_patrolState = new EnemyPatrolState(this);
        m_chaseState = new EnemyChaseState(this);
        m_attackState = new EnemyAttackState(this);

        // link states together
        m_patrolState.ChaseState = m_chaseState;
        m_chaseState.PatrolState = m_patrolState;
        m_chaseState.AttackState = m_attackState;
        m_attackState.ChaseState = m_chaseState;

        //init state machine with it starting at patrolState
        m_fsm = new HFSM();
        m_fsm.Initialize(m_patrolState);
    }

    //getter for enemy useful helper
    public Entity GetVisualEntity()
    {
        return m_visualEntity;
    }

    public bool CanSeePlayer()
    {
        //might wanna use this for monsters that require LOS
    }

    public float GetDistanceToPlayer()
    {
        //early exit if player doesnt exist
        // if player entity doesnt exist set to max so the check distance will fail = no change state
        if(m_playerEntity.EntityId == null)
        {
            return float.MaxValue;
        }
        // early exit if enemy doesnt move
        if (!m_visualEntity.TryGetComponent<LocalTransform>(out var myTransform))
            return float.MaxValue;

        // preventative incase entity is created but no transform is set if player is nearby
        if (!m_playerEntity.TryGetComponent<LocalTransform>(out var playerTransform))
        {
            return float.MaxValue;
        }

        //diff gets the difference vector of values
        var diff = playerTransform.Position - myTransform.Position;
        // .magnitude converts it into distance
        var distance = diff.magnitude;

        return distance;
    }

    public void MoveInDirection(Vector3 direction, float deltaTime)
    {
        // for static enemies (example like grabber kinds) because they dont need to move
        if (!m_visualEntity.TryGetComponent<LocalTransform>())
            return;

            /*
           * a-star to be set here
           */

    }

    public void MoveTowardPlayer(float deltaTime)
    {
        //earlyexit if player doesnt exist
        if (m_playerEntity.EntityId == 0)
            return;
        
        // if player doesnt have transform set
        if (!m_playerEntity.TryGetComponent<LocalTransform>(out var myTransform))
            return;

        //early exit for non moving enemies
        if (!m_visualEntity.TryGetComponent<LocalTransform>(out var myTransform))
            return;

        if (GetDistanceToPlayer() < m_detectionRadius)
        {
            // direction - direction vector 
            var direction = playerTransform.Position - myTransform.Position;
            MoveInDirection(direction, deltaTime);
        }
    }

    // this helps enemy know who the player is 
    public void SetPlayerEntity(Entity player)
    {
        m_playerEntity = player;
    }


    public void DealDamageToPlayer(float damage)
    {
        // todo hp system or maybe elsewhere. whereever the damage can apply quicker.
        Log($"Attacking player for {damage} damage!", LogLevel.Info);
    }

    private void OnDeath()
    {
        // maybe for bosses or if this AI has to die for the player to progress
        Log("Enemy defeated!", LogLevel.Info);
        m_visualEntity.Destroy();
        DestroyEntity();
    }

}


/*
 * STATES
 */


public class EnemyPatrolState : State
{
    // set enemy for this state 
    private EnemyAI m_enemy;

    //patrol points
    private Vector3 m_patrolPointA;
    private Vector3 m_patrolPointB;

    // flag to help set patrollings
    private bool m_movingToB = true;
    private float m_detectionR = 150.0f;

    //linkage from patrol -> chase
    public EnemyChaseState ChaseState;

    // constructor required to create state
    public EnemyPatrolState(EnemyAI enemy)
    {
        m_enemy = enemy;
    }

    //once patrolling state starts
    public override void OnEnter()
    {
        
        // set visualentity to menemy of current state
        var entity = m_enemy.GetVisualEntity();

        // if enemy is of move type set patrol points based on current position
        if (entity.TryGetComponent<LocalTransform>(out var transform))
        {
            // needs to be modified later presumably
            m_patrolPointA = transform.Position;
            m_patrolPointB = transform.Position + new Vector3(200.0f, 0.0f, 0.0f);
        }
    }

    // on update patrol
    public override void OnUpdate(float deltaTime)
    {
        // set visualentity to menemy of current state
        var entity = m_enemy.GetVisualEntity();

        //early return if enemy cant even move
        if (!entity.TryGetComponent<LocalTransform>(out var transform))
            return;

        // Simple patrol between two points
        var targetPoint = m_movingToB ? m_patrolPointB : m_patrolPointA;
        var direction = targetPoint - transform.Position;
        var distance = direction.Magnitude;

        // Move towards patrol point
        if (distance > 5.0f)
        {
            m_enemy.MoveInDirection(direction, deltaTime);
        }
        else
        {
        // todo - set facing direction function somewhats heresss just a simple flip would do
        // Reached patrol point, switch direction
            m_movingToB = !m_movingToB;
        }
    }

    public override void OnExit()
    {
      // for possible behaviours
    }

    public override State CheckTransitions()
    {
        // if player detected -> chase
        if (m_enemy.GetDistanceToPlayer() < m_detectionR)
        {
            return ChaseState;
        }

        return null;
    }
}


public class EnemyChaseState : State
{
    //reference to enemy in this state
    private EnemyAI m_enemy;

    //needed for state linkage
    public EnemyPatrolState PatrolState;
    public EnemyAttackState AttackState;

    //constructor to create state
    public EnemyChaseState(EnemyAI enemy)
    {
        m_enemy = enemy;
    }

    // to set speed once state changes
    public override void OnEnter()
    {
        //chase speed = base speed x 1.5
        m_enemy.Speed = m_enemy.BaseSpeed * 1.5f; // Speed boost when chasing
    }

    // on update enemy chases player
    public override void OnUpdate(float deltaTime)
    {
        m_enemy.MoveTowardPlayer(deltaTime);
    }

    // on exit reset speed
    public override void OnExit()
    {
        m_enemy.Speed = m_enemy.BaseSpeed; // Reset speed
    }

    // state transitionings
    public override State CheckTransitions()
    {
        float distance = m_enemy.GetDistanceToPlayer();

        // Close enough to attack
        if (distance <= m_enemy.AttackRange)
        {
            return AttackState;
        }

        // Lost player (too far away)
        if (distance > m_enemy.DetectionRange * 1.5f)
        {
            return PatrolState;
        }

        return null;
    }
}


public class EnemyAttackState : State
{
    //reference to enemy in this state
    private EnemyAI m_enemy;

    // buffer
    private float m_attackCooldown = 0f;

    //resetter
    private const float ATTACK_COOLDOWN_TIME = 1.5f;

    //needed for state linkage
    public EnemyChaseState ChaseState;

    // constructor to create state
    public EnemyAttackState(EnemyAI enemy)
    {
        m_enemy = enemy;
    }

    // once attack state triggers 
    public override void OnEnter()
    {
        //make sure if state has changed -> attack it can deal damage
        m_attackCooldown = 0f;
    }

    public override void OnUpdate(float deltaTime)
    {
        // to make sure that cool down is always refreshed if enemy in attack state
        m_attackCooldown -= deltaTime;
        
        //if cd < 0 and in attack state deal damage 
        if (m_attackCooldown <= 0f)
        {
            m_enemy.DealDamageToPlayer(m_enemy.AttackDamage);

            // once damage dealt set cool down
            m_attackCooldown = ATTACK_COOLDOWN_TIME;
        }
    }

    public override void OnExit()
    {
        // if require to set other behaviours here
    }

    //state transitionings
    public override State CheckTransitions()
    {
        float distance = m_enemy.GetDistanceToPlayer();

        // Player escaped attack range
        if (distance > m_detectionRadius)
        {
            return ChaseState;
        }

        return null;
    }
}