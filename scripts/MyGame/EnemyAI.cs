/**
* @Name: Dalton koh, 2403250. Daniel Neo, 2401180
* @email: d.koh@digipen.edu, k.danielneozuofeng@digipen.edu
* @file EnemyAI.cs
* @brief Enemy HFSM with patrol, chase, and attack; A* path following.
*
* @details
* Implements a hierarchical finite state machine (HFSM) for enemy behavior that
* uses A* paths to chase the player around scripted obstacles. Key pieces:
* - Visual entity creation and animated feedback (throb/color by state)
* - Cached obstacle visuals built from AStarPathfinder list
* - Patrol/Chase/Attack states with transitions on player distance
* - Path maintenance with periodic refresh and anti-stuck timer
* - Wall sliding fallback when pathfinding fails
*
* @dependencies
* - GrapeEngine scripting/Entity APIs, Numerics vectors
* - AStarPathfinder for pathfinding and obstacle dimensions
* 
* Copyright (C) 2025 DigiPen Institute of Technology.
* Reproduction or disclosure of this file or its contents without the
*/

using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using System.Collections.Generic;

namespace MyGame;

public class EnemyAI : ScriptBehaviour
{
    //AI state machine
    private HFSM m_fsm = null!;

    // stats
    public float BaseSpeed = 80.0f;
    public float Speed = 80.0f;
    public float AttackDamage = 10.0f;

    // incase we need it for specific game scenes
    public float Health = 100f;
    public float MaxHealth = 100f;

    // visual entity of enemy
    private Entity m_visualEntity = null!;
    private List<Entity> m_obstacleEntities = new List<Entity>();

    // state references so it can be reusable
    private EnemyPatrolState m_patrolState  = null!;
    private EnemyChaseState m_chaseState    = null!;
    private EnemyAttackState m_attackState  = null!;

    // pathfinding stuff - for chasing the player
    private List<Vector3> m_currentPath         = null!;
    private int m_currentWaypointIndex          = 0;
    private float m_pathUpdateTimer             = 0f;
    private const float PATH_UPDATE_INTERVAL    = 0.5f;        // how often to recalculate path
    private const float WAYPOINT_REACH_DISTANCE = 15.0f;    // how close we need to get to the waypoint

    // anti-stuck mechanism - detects if enemy isn't moving
    private Vector3 m_lastPosition;
    private float m_stuckTimer = 0f;
    private const float STUCK_THRESHOLD = 0.5f;             // seconds before we consider enemy stuck

    // debug visualization
    private List<Entity> m_pathDebugEntities = new List<Entity>();

    // Player reference (need to read game manager probably info there)
    private Entity m_playerEntity = null!;
    //bool flag for playerset
    private bool m_linkedToPlayer = false;

    // cache obstacles
    private List<Vector3> m_cachedObstacles = null!;

    public override void OnStart()
    {
        Log("EnemyAI initialized!", LogLevel.Info);

        // make sure the pathfinder knows all the walls
        AStarPathfinder.EnsureInitialized();
        m_cachedObstacles = AStarPathfinder.GetObstacles();

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


        ref var transform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var Transform);
        if(Transform)
        {
            m_lastPosition = transform.Position;
        }

        Log($"Enemy visual entity created: {m_visualEntity.EntityId}", LogLevel.Info);

        // shows the walls as red rectangles
        CreateObstacleVisuals();

        // build state machine
        BuildStateMachine();
        TryLinkToPlayer();

        Log("Enemy ready with HFSM~!");
    }

    private void CreateObstacleVisuals()
    {
        // Log($"Creating {m_cachedObstacles.Count} obstacle visuals...", LogLevel.Info); // debug

        if (m_cachedObstacles.Count == 0)
        {
            return;
        }

        // get how big the wall blocks should be
        var (width, height) = AStarPathfinder.GetObstacleDimensions();

        // create a red rectangle for each wall position
        foreach (var obstaclePos in m_cachedObstacles)
        {
            var obstacleEntity = CreateEntity(
                new ComponentData<LocalTransform>(new()
                {
                    Position = obstaclePos,
                    Rotation = Quaternion.Identity,
                    Scale = new Vector3(1, 1, 1)
                }),
                new ComponentData<ShapeBox2D>(new()
                {
                    HalfExtents = new Vector2(width / 2, height / 2),
                    Offset = Vector2.Zero,
                    Color = new Color { R = 0.8f, G = 0.2f, B = 0.2f, A = 1f },
                    Filled = true
                }),
                new ComponentData<Layer>(new() { Id = 0 }),
                new ComponentData<Active>(new() { Enabled = true })
            );
            m_obstacleEntities.Add(obstacleEntity);
        }
        // Log($"Created {m_obstacleEntities.Count} rectangular obstacle visuals", LogLevel.Info); // debug
    }

    public override void OnUpdate()
    {

        // Try to link to player if not already linked
        if (!m_linkedToPlayer)
        {
            TryLinkToPlayer();
        }

        // Only update FSM if linked to player
        if (m_linkedToPlayer)
        {

            // Update the state machine - basically runs the ai states
            m_fsm.Update(Time.DeltaTime);

            // Update visuals - enemy visuals
            UpdateEnemyVisual();

            // update path visual - debugging
            UpdatePathDebugVisuals();
        }
    }

    private void UpdateEnemyVisual()
    {
        // Make enemy throb to show it's active
   
        ref var shapeCircle = ref m_visualEntity.TryGetComponent<ShapeCircle2D>(out var circle);
        if (!circle) return;

        var throb = 0.85f + 0.15f * MathF.Sin((float)Time.ElapsedTime * 3.0f);
        shapeCircle.Radius = 15.0f * throb;


        // Change color based on current state

        if (m_fsm.CurrentState is EnemyAttackState)
        {
            // Check cooldown status
            if (m_attackState.m_attackCooldown > 0.0f)
            {
                //  BLACK when on cooldown (can't attack yet)
                shapeCircle.Color.R = 0.0f;
                shapeCircle.Color.G = 0.0f;
                shapeCircle.Color.B = 0.0f;
                shapeCircle.Color.A = 1.0f;
            }
            else
            {
                // WHITE when ready to attack (cooldown finished)
                shapeCircle.Color.R = 1.0f;
                shapeCircle.Color.G = 1.0f;
                shapeCircle.Color.B = 1.0f;
                shapeCircle.Color.A = 1.0f;
            }
        }

        else if (m_fsm.CurrentState is EnemyChaseState)
        {
            // Orange when chasing
            shapeCircle.Color.R = 1.0f;
            shapeCircle.Color.G = 0.5f;
            shapeCircle.Color.B = 0.0f;
            shapeCircle.Color.A = 1.0f;
        }
        else
        {
            // Red color when patrolling (default)
            shapeCircle.Color.R = throb;
            shapeCircle.Color.G = 0.0f;
            shapeCircle.Color.B = 0.0f;
            shapeCircle.Color.A = 1.0f;
        }
    }

    // shows circles along the current path of enemy - debugging!
    private void UpdatePathDebugVisuals()
    {
        // clear old debug visuals
        foreach (var entity in m_pathDebugEntities)
        {
            entity.Destroy();
        }
        m_pathDebugEntities.Clear();

        // create new debug visuals for current path
        if (m_currentPath != null && m_currentPath.Count > 0)
        {
            for (int i = 0; i < m_currentPath.Count; i++)
            {
                bool isCurrent = (i == m_currentWaypointIndex);

                var debugEntity = CreateEntity(
                    new ComponentData<LocalTransform>(new()
                    {
                        Position = m_currentPath[i],
                        Rotation = Quaternion.Identity,
                        Scale = new Vector3(1, 1, 1)
                    }),
                    new ComponentData<ShapeCircle2D>(new()
                    {
                        Radius = isCurrent ? 8.0f : 5.0f,   // current waypoint is bigger?
                        Color = isCurrent
                            ? new Color { R = 0f, G = 1f, B = 0f, A = 0.8f }    // green
                            : new Color { R = 1f, G = 1f, B = 0f, A = 0.5f },   // yellow
                        Filled = true
                    }),
                    new ComponentData<Layer>(new() { Id = 0 }),
                    new ComponentData<Active>(new() { Enabled = true })
                );
                m_pathDebugEntities.Add(debugEntity);
            }
        }
    }

    // Function to bind to the player's visual entity if available
    private void TryLinkToPlayer()
    {
        if (PlayerController.Instance != null)
        {
           // Set playerentitiy to visual enttiy
            SetPlayerEntity(PlayerController.Instance.GetVisualEntity());
            // set bool to true 
            m_linkedToPlayer = true;

        }

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
        m_attackState.PatrolState = m_patrolState;

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
        //might wanna use this for monsters that require LOS <- todo for more detailed implementation

        // Check if player entity is valid before accessing EntityId
        return m_playerEntity != default && m_playerEntity.EntityId != 0;
    }

    public float GetDistanceToPlayer()
    {

        //early exit if player doesnt exist
        // if player entity doesnt exist set to max so the check distance will fail = no change state
        if (m_playerEntity == default || m_playerEntity.EntityId == 0)
        {
            return float.MaxValue;
        }
      
        // early exit if enemy doesnt move
        ref var mytransform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var myTransform);
        if (!myTransform) return float.MaxValue;

        // preventative incase entity is created but no transform is set if player is nearby
        ref var playertransform = ref m_playerEntity.TryGetComponent<LocalTransform>(out var playerTransform);
        if (!playerTransform) return float.MaxValue;

        //diff gets the difference vector of values
        var diff = playertransform.Position - mytransform.Position;
        // .magnitude converts it into distance
        var distance = diff.Magnitude;

        return distance;
    }

    // simple movement in a direction (used for patrolling)
    public void MoveInDirection(Vector3 direction, float deltaTime)
    {
 
        ref var transform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var myTransform);
        if (!myTransform) return;

        if (direction.Magnitude < 0.01f)
            return;

        // calculate movement and new position
        Vector3 movement = direction.Normalized * Speed * deltaTime;
        Vector3 newPosition = transform.Position + movement;

        // only move if we wont hit a wall
        if (!WouldCollideWithObstacle(newPosition))
        {
            transform.Position = newPosition;
        }
    }

    // smart movement towards player using pathfinding
    public void MoveTowardPlayer(float deltaTime)
    {
        //earlyexit if player doesnt exist
        if (m_playerEntity == default || m_playerEntity.EntityId == 0)
            return;

        // if player doesnt have transform set
        ref var playertransform = ref m_playerEntity.TryGetComponent<LocalTransform>(out var playerTransform);
        if (!playerTransform) return;

        ref var mytransform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var myTransform);
        if (!myTransform) return;

        // if (GetDistanceToPlayer() < m_detectionRadius)

        // stuck detection (thsi will check if enemy is actually moving)
        m_pathUpdateTimer -= deltaTime;

        float distanceMoved = (mytransform.Position - m_lastPosition).Magnitude;

        if (distanceMoved > 1.0f * deltaTime)
        {
            m_stuckTimer = 0f;  // we are moving, resets stuck timer
        }
        else
        {
            // direction - direction vector 
            // var direction = playerTransform.Position - myTransform.Position;
            // MoveInDirection(direction, deltaTime);
            m_stuckTimer += deltaTime;  // we are not moving, counts as stuck
        }

        m_lastPosition = mytransform.Position;

        // do we need a new path?
        bool needsNewPath = m_currentPath == null ||
                           m_currentPath.Count == 0 ||
                           m_currentWaypointIndex >= m_currentPath.Count ||
                           m_pathUpdateTimer <= 0f ||
                           m_stuckTimer > STUCK_THRESHOLD;

        if (needsNewPath)
        {
            // ask pathfinder for a new route to player
            var newPath = AStarPathfinder.FindPath(mytransform.Position, playertransform.Position);

            if (newPath != null && newPath.Count > 0)
            {
                m_currentPath = newPath;
                m_currentWaypointIndex = 0;
                m_pathUpdateTimer = PATH_UPDATE_INTERVAL;
                m_stuckTimer = 0f;
            }
            else
            {
                // no path found! - try to move directly with wall sliding
                m_currentPath = new List<Vector3>();
                MoveWithWallSliding(playertransform.Position, ref mytransform, deltaTime);
                return;
            }
        }

        // follows the current path
        if (m_currentPath != null && m_currentWaypointIndex < m_currentPath.Count)
        {
            Vector3 targetWaypoint = m_currentPath[m_currentWaypointIndex];
            Vector3 toWaypoint = targetWaypoint - mytransform.Position;
            float distanceToWaypoint = toWaypoint.Magnitude;

            // move to next waypoint if we are close enough
            if (distanceToWaypoint < WAYPOINT_REACH_DISTANCE)
            {
                m_currentWaypointIndex++;
                return;
            }

            // move toward current waypoint with wall sliding
            MoveWithWallSliding(targetWaypoint, ref mytransform, deltaTime);
        }
    }

    // tries to move towards target, slides along wall if blocked
    private void MoveWithWallSliding(Vector3 target, ref LocalTransform transform, float deltaTime)
    {
        Vector3 direction = (target - transform.Position).Normalized;
        Vector3 movement = direction * Speed * deltaTime;
        Vector3 newPosition = transform.Position + movement;

        // try direct movement first
        if (!WouldCollideWithObstacle(newPosition))
        {
            transform.Position = newPosition;
            return;
        }

        // if blocked, try sliding along the wall
        Vector3 slideDir = GetSlideDirection(transform.Position, direction);
        if (slideDir.Magnitude > 0.001f)
        {
            Vector3 slideMove = slideDir * Speed * deltaTime;
            Vector3 slidePos = transform.Position + slideMove;

            if (!WouldCollideWithObstacle(slidePos))
            {
                transform.Position = slidePos;
                return;
            }
        }

        // if still stuck, back up a bit
        Vector3 backStep = new Vector3(-direction.X, -direction.Y, -direction.Z) * (Speed * deltaTime * 0.5f);
        Vector3 backPos = transform.Position + backStep;
        if (!WouldCollideWithObstacle(backPos))
        {
            transform.Position = backPos;
        }
    }

    // figures out which direction to slide when blocked
    private Vector3 GetSlideDirection(Vector3 position, Vector3 direction)
    {
        // check if we are blocked in X or Y direction
        bool blockedX = WouldCollideWithObstacle(position + new Vector3(direction.X * 10f, 0, 0));
        bool blockedY = WouldCollideWithObstacle(position + new Vector3(0, direction.Y * 10f, 0));

        // slide in the unblocked direction
        if (blockedX && !blockedY)
            return new Vector3(0, direction.Y, 0);
        if (blockedY && !blockedX)
            return new Vector3(direction.X, 0, 0);

        return Vector3.Zero;
    }

    // checks if a position would hit any walls
    private bool WouldCollideWithObstacle(Vector3 newPosition)
    {
        var (obstacleWidth, obstacleHeight) = AStarPathfinder.GetObstacleDimensions();

        float enemyHalfSize = 10.0f; // how big the enemy is
        float obstacleHalfWidth = obstacleWidth / 2f;
        float obstacleHalfHeight = obstacleHeight / 2f;

        // check against all walls
        foreach (var obstacle in m_cachedObstacles)
        {
            bool collisionX = Math.Abs(newPosition.X - obstacle.X) < (enemyHalfSize + obstacleHalfWidth);
            bool collisionY = Math.Abs(newPosition.Y - obstacle.Y) < (enemyHalfSize + obstacleHalfHeight);

            if (collisionX && collisionY)
            {
                return true;    // would hit a wall
            }
        }
        return false;   // clear path
    }

    // this helps enemy know who the player is 
    public void SetPlayerEntity(Entity player)
    {
        m_playerEntity = player;
    }


    public void DealDamageToPlayer(float damage)
    {
        // todo hp system or maybe elsewhere. whereever the damage can apply quicker.
    }

    private void OnDeath()
    {
        // maybe for bosses or if this AI has to die for the player to progress
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
    private float m_detectionR = 350.0f;   // important to set big enough for pathfinding to work

    //minimum distance 
    private float m_minD = 5.0f;

    //linkage from patrol -> chase
    public EnemyChaseState? ChaseState;

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

        //if (entity.TryGetComponent<LocalTransform>(out var transform))
        //{
        //    // needs to be modified later presumably
        //    m_patrolPointA = transform.Position;
        //    m_patrolPointB = transform.Position + new Vector3(200.0f, 0.0f, 0.0f);
        //}


        //// if enemy is of move type set patrol points based on current position
        ref var transform = ref entity.TryGetComponent<LocalTransform>(out var hasTransform);
        if (hasTransform)
        {
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
        ref var transform = ref entity.TryGetComponent<LocalTransform>(out var hasTransform);
        if (!hasTransform) return;

        // Simple patrol between two points
        var targetPoint = m_movingToB ? m_patrolPointB : m_patrolPointA;
        var direction = targetPoint - transform.Position;
        var distance = direction.Magnitude;

        // Move towards patrol point
        if (distance > m_minD)
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
            return ChaseState!;
        }

        return null!;
    }
}


public class EnemyChaseState : State
{
    //reference to enemy in this state
    private EnemyAI m_enemy;

    //needed for state linkage
    public EnemyPatrolState? PatrolState;
    public EnemyAttackState? AttackState;

    //ranges
    private float AttackRange = 5.0f; //placeholder
    private float DetectionRange = 350.0f;

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
        if (distance <= AttackRange)
        {
            return AttackState!;
        }

        // Lost player (too far away)
        if (distance > DetectionRange)
        {
            return PatrolState!;
        }

        return null!;
    }
}


public class EnemyAttackState : State
{
    //reference to enemy in this state
    private EnemyAI m_enemy;

    // buffer
    public float m_attackCooldown = 0f;

    //detection radius
    private float AttackRange = 5.0f;
    private float DetectionRange = 350.0f;

    //resetter
    private const float ATTACK_COOLDOWN_TIME = 1.5f;

    //needed for state linkage
    public EnemyChaseState? ChaseState;
    public EnemyPatrolState? PatrolState;

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

        //if dist less then attack range continue state
        if (distance <= AttackRange)
        {
            return null!;
        }
        // if distance less then detection range and more then attack range change to chase state
        else if (distance <= DetectionRange && distance >= AttackRange)
        {
            return ChaseState!;
        }
        else
        {
        // other wise just back to patrol because totally out of range
            return PatrolState!;
        }

        //  return null;    // unreachable
    }
}