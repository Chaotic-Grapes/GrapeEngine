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
* prior written consent of DigiPen Institute of Technology is prohibited.
*/

using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;
using System.Collections.Generic;

namespace MyGame;


/// Enemy AI Controller - Decision Maker and Game Logic Handler
public class EnemyAI : ScriptBehaviour
{
    // Stats - still got some stuff TO-DO alot of old stuff
    public float BaseSpeed = 80.0f;
    public float Speed = 80.0f;
    public float AttackDamage = 10.0f;
    public float Health = 100f;
    public float MaxHealth = 100f;

    // C++ brain (State container - created and managed by C#)
    private Brain m_brain = null!;

    // We cache these so we can quickly compare which state we're in
    // without calling P/Invoke every time
    private IntPtr m_patrolStatePtr = IntPtr.Zero;
    private IntPtr m_chaseStatePtr = IntPtr.Zero;
    private IntPtr m_attackStatePtr = IntPtr.Zero;


    // Decision ranges (use different ranges to typically decide which state to go)
    private float m_detectionRange = 350.0f;   // Patrol -> Chase distance
    private float m_attackRange = 5.0f;         // Chase -> Attack distance
    private float m_loseDistance = 350.0f;      // Chase -> Patrol distance (player too far)

    // Visual entities (walls & main entity)
    private Entity m_visualEntity = null!;
    private List<Entity> m_obstacleEntities = new List<Entity>();
    private List<Entity> m_pathDebugEntities = new List<Entity>();

    // Pathfinding part to be edited
    private List<Vector3> m_currentPath = null!;
    private int m_currentWaypointIndex = 0;
    private float m_pathUpdateTimer = 0f;
    private const float PATH_UPDATE_INTERVAL = 0.5f;
    private const float WAYPOINT_REACH_DISTANCE = 15.0f;

    // Anti stuck mechanism
    private Vector3 m_lastPosition;
    private float m_stuckTimer = 0f;
    private const float STUCK_THRESHOLD = 0.5f;

    // OLD code - attack CD placeholder
    private float m_attackCooldown = 0f;
    private const float ATTACK_COOLDOWN_TIME = 1.5f;

    // Player link
    private Entity? m_playerEntity = null;
    private bool m_linkedToPlayer = false;

    // OLD code - cached obstacles. Not super sure about this 
    private List<Vector3> m_cachedObstacles = null!;

    // OLD code -Patrol points to be saved 
    private Vector3 m_patrolPointA;
    private Vector3 m_patrolPointB;
    private bool m_movingToB = true;
    private const float PATROL_MIN_DISTANCE = 5.0f;

    // Lifecycles
    public override void OnStart()
    {

        // Initialize pathfinder and get cached obstacles
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
            new ComponentData<ShapeCircle2D>(new()
            {
                Radius = 15.0f,
                Color = new Color { R = 1f, G = 0f, B = 0f, A = 1f },  // Red
                Filled = true
            }),
            new ComponentData<Layer>(new() { Id = 0 }),
            new ComponentData<Active>(new() { Enabled = true })
        );

        ref var transform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var Transform);
        if (Transform)
        {
            m_lastPosition = transform.Position;
            m_patrolPointA = transform.Position;
            m_patrolPointB = transform.Position + new Vector3(200.0f, 0.0f, 0.0f);
        }

        // Create visual representations of obstacles
        CreateObstacleVisuals();

        // now create the C++ BRAIN (this is where the FSM lives)
        // to do state transition work
        m_brain = new Brain();

        // Cache the state pointers for efficient comparisons
        m_patrolStatePtr = m_brain.GetPatrolState();
        m_chaseStatePtr = m_brain.GetChaseState();
        m_attackStatePtr = m_brain.GetAttackState();

        // Start in Patrol state
        m_brain.TransitionTo(m_patrolStatePtr); 

        // Try to link to player
        TryLinkToPlayer();
    }

    public override void OnUpdate()
    {
        // Try to link to player if not already linked
        if (!m_linkedToPlayer)
        {
            TryLinkToPlayer();
        }

        // Only update if linked to player
        if (m_linkedToPlayer && m_brain != null)
        {
            // Core Looping for state transitions 
            // This is where C# checks conditions and tells C++ to transition
            CheckAndMakeTransitions();

            // Execute state-specific logic in C#
            ExecuteCurrentStateLogic();

            // Update visuals
            UpdateEnemyVisual();
            UpdatePathDebugVisuals();
        }
    }

    // Decision making (C# is the intelligent decision maker)
    /// Check and make transitions part of things
    private void CheckAndMakeTransitions()
    {
        float distance = GetDistanceToPlayer();
        IntPtr currentState = m_brain.GetCurrentState();

        if (currentState == m_patrolStatePtr)
        {
            if (distance < m_detectionRange)
            {
            //   Log($"[Patrol->Chase] Player detected at distance {distance:F1}", LogLevel.Info);
                m_brain.TransitionTo(m_chaseStatePtr);
                return;  // Only one transition per frame
            }
        }


        // CHASE-> ATTACK or CHASE -> PATROL
        else if (currentState == m_chaseStatePtr)
        {
            if (distance <= m_attackRange)
            {
            //    Log($"[Chase -> Attack] Player in range at distance {distance:F1}", LogLevel.Info);
                m_brain.TransitionTo(m_attackStatePtr);
                return;
            }
            else if (distance > m_loseDistance)
            {
             //   Log($"[Chase -> Patrol] Player lost at distance {distance:F1}", LogLevel.Info);
                m_brain.TransitionTo(m_brain.GetPatrolState());
                return;
            }
        }

        // ATTACK-> CHASE or ATTACK -> PATROL
        else if (currentState == m_attackStatePtr)
        {
            if (distance > m_attackRange + 10.0f)  // Small buffer zone
            {
              //  Log($"[Attack -> Chase] Player moved away to distance {distance:F1}", LogLevel.Info);
                m_brain.TransitionTo(m_chaseStatePtr);
                return;
            }
            else if (distance > m_loseDistance)
            {
                // Log($"[Attack -> Patrol] Player completely lost at distance {distance:F1}", LogLevel.Info);
                m_brain.TransitionTo(m_patrolStatePtr);
                return;
            }
        }
    }

    /// ExecuteCurrentStateLogic
    private void ExecuteCurrentStateLogic()
    {
        State currentState = m_brain.GetCurrentState();

        if (currentState == m_brain.GetPatrolState())
        {
            ExecutePatrolLogic();
        }
        else if (currentState == m_brain.GetChaseState())
        {
            ExecuteChaseLogic();
        }
        else if (currentState == m_brain.GetAttackState())
        {
            ExecuteAttackLogic();
        }
    }


    // STATE BEHAVIOURS

    /// Execute patrol logic
    private void ExecutePatrolLogic()
    {
        var entity = m_visualEntity;

        ref var transform = ref entity.TryGetComponent<LocalTransform>(out var hasTransform);
        if (!hasTransform) return;

        // Simple patrol between two points
        var targetPoint = m_movingToB ? m_patrolPointB : m_patrolPointA;
        var direction = targetPoint - transform.Position;
        var distance = direction.Magnitude;

        // Move towards patrol point
        if (distance > PATROL_MIN_DISTANCE)
        {
            MoveInDirection(direction, Time.DeltaTime);
        }
        else
        {
            // Reached patrol point, switch direction
            m_movingToB = !m_movingToB;
        }
    }


    /// Execute Chase
    private void ExecuteChaseLogic()
    {
        // Speed boost when chasing
        Speed = BaseSpeed * 1.5f;

        // Move toward player using pathfinding
        MoveTowardPlayer(Time.DeltaTime);
    }

    // Execute Attack
    private void ExecuteAttackLogic()
    {
        // Reset speed to normal
        Speed = BaseSpeed;

        // Handle attack cooldown
        m_attackCooldown -= Time.DeltaTime;

        // Deal damage if cooldown is ready
        if (m_attackCooldown <= 0f)
        {
            DealDamageToPlayer(AttackDamage);
            m_attackCooldown = ATTACK_COOLDOWN_TIME;
        }
    }


    // Movement helper functions
    private void MoveInDirection(Vector3 direction, float deltaTime)
    {
        ref var transform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var myTransform);
        if (!myTransform) return;

        if (direction.Magnitude < 0.01f) return;

        Vector3 movement = direction.Normalized * Speed * deltaTime;
        Vector3 newPosition = transform.Position + movement;

        if (!WouldCollideWithObstacle(newPosition))
        {
            transform.Position = newPosition;
        }
    }

    // movement helper functions 
    private void MoveTowardPlayer(float deltaTime)
    {
        if (m_playerEntity == default || m_playerEntity.EntityId == 0) return;

        ref var playertransform = ref m_playerEntity.TryGetComponent<LocalTransform>(out var playerTransform);
        if (!playerTransform) return;

        ref var mytransform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var myTransform);
        if (!myTransform) return;

        m_pathUpdateTimer -= deltaTime;
        float distanceMoved = (mytransform.Position - m_lastPosition).Magnitude;

        if (distanceMoved > 1.0f * deltaTime)
        {
            m_stuckTimer = 0f;
        }
        else
        {
            m_stuckTimer += deltaTime;
        }

        m_lastPosition = mytransform.Position;

        bool needsNewPath = m_currentPath == null ||
                           m_currentPath.Count == 0 ||
                           m_currentWaypointIndex >= m_currentPath.Count ||
                           m_pathUpdateTimer <= 0f ||
                           m_stuckTimer > STUCK_THRESHOLD;

        if (needsNewPath)
        {
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
                m_currentPath = new List<Vector3>();
                MoveWithWallSliding(playertransform.Position, ref mytransform, deltaTime);
                return;
            }
        }

        if (m_currentPath != null && m_currentWaypointIndex < m_currentPath.Count)
        {
            Vector3 targetWaypoint = m_currentPath[m_currentWaypointIndex];
            Vector3 toWaypoint = targetWaypoint - mytransform.Position;
            float distanceToWaypoint = toWaypoint.Magnitude;

            if (distanceToWaypoint < WAYPOINT_REACH_DISTANCE)
            {
                m_currentWaypointIndex++;
                return;
            }

            MoveWithWallSliding(targetWaypoint, ref mytransform, deltaTime);
        }
    }

    // same thing but taking into account part for the wall
    private void MoveWithWallSliding(Vector3 target, ref LocalTransform transform, float deltaTime)
    {
        Vector3 direction = (target - transform.Position).Normalized;
        Vector3 movement = direction * Speed * deltaTime;
        Vector3 newPosition = transform.Position + movement;

        if (!WouldCollideWithObstacle(newPosition))
        {
            transform.Position = newPosition;
            return;
        }

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

        Vector3 backStep = new Vector3(-direction.X, -direction.Y, -direction.Z) * (Speed * deltaTime * 0.5f);
        Vector3 backPos = transform.Position + backStep;
        if (!WouldCollideWithObstacle(backPos))
        {
            transform.Position = backPos;
        }
    }

    // Old code from daniel side 
    private Vector3 GetSlideDirection(Vector3 position, Vector3 direction)
    {
        bool blockedX = WouldCollideWithObstacle(position + new Vector3(direction.X * 10f, 0, 0));
        bool blockedY = WouldCollideWithObstacle(position + new Vector3(0, direction.Y * 10f, 0));

        if (blockedX && !blockedY)
            return new Vector3(0, direction.Y, 0);
        if (blockedY && !blockedX)
            return new Vector3(direction.X, 0, 0);

        return Vector3.Zero;
    }

    // ~~ 
    private bool WouldCollideWithObstacle(Vector3 newPosition)
    {
        var (obstacleWidth, obstacleHeight) = AStarPathfinder.GetObstacleDimensions();

        float enemyHalfSize = 10.0f;
        float obstacleHalfWidth = obstacleWidth / 2f;
        float obstacleHalfHeight = obstacleHeight / 2f;

        foreach (var obstacle in m_cachedObstacles)
        {
            bool collisionX = Math.Abs(newPosition.X - obstacle.X) < (enemyHalfSize + obstacleHalfWidth);
            bool collisionY = Math.Abs(newPosition.Y - obstacle.Y) < (enemyHalfSize + obstacleHalfHeight);

            if (collisionX && collisionY)
            {
                return true;
            }
        }
        return false;
    }

    
    // Get distance from hero to AI method
    public Entity GetVisualEntity() => m_visualEntity;

    public float GetDistanceToPlayer()
    {
        if (m_playerEntity == default || m_playerEntity.EntityId == 0)
            return float.MaxValue;

        ref var mytransform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var myTransform);
        if (!myTransform) return float.MaxValue;

        ref var playertransform = ref m_playerEntity.TryGetComponent<LocalTransform>(out var playerTransform);
        if (!playerTransform) return float.MaxValue;

        var diff = playertransform.Position - mytransform.Position;
        return diff.Magnitude;
    }

    // Place holder stuff 
    public void DealDamageToPlayer(float damage)
    {
        // TODO -> Implement damage system
        // This is where the actual damage is applied to the player
    }

    // Method for player link
    private void TryLinkToPlayer()
    {
        if (PlayerController.Instance != null)
        {
            SetPlayerEntity(PlayerController.Instance.GetVisualEntity());
            m_linkedToPlayer = true;
        }
    }

    // Entity set to player type
    public void SetPlayerEntity(Entity? player)
    {
        m_playerEntity = player;
    }


    // Obstacle visuals 

    private void CreateObstacleVisuals()
    {
        if (m_cachedObstacles.Count == 0)
            return;

        var (width, height) = AStarPathfinder.GetObstacleDimensions();

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
    }

    private void UpdateEnemyVisual()
    {
        ref var shapeCircle = ref m_visualEntity.TryGetComponent<ShapeCircle2D>(out var circle);
        if (!circle) return;

        var throb = 0.85f + 0.15f * MathF.Sin((float)Time.ElapsedTime * 3.0f);
        shapeCircle.Radius = 15.0f * throb;

        // Get current state from C++ Brain
        var currentState = m_brain.GetCurrentState();

        // Change color based on current state
        if (currentState == m_attackStatePtr) 
        {
            // Attack state: White when ready, Black when on cooldown
            if (m_attackCooldown > 0.0f)
            {
                shapeCircle.Color = new Color { R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f };  // Black
            }
            else
            {
                shapeCircle.Color = new Color { R = 1.0f, G = 1.0f, B = 1.0f, A = 1.0f };  // White
            }
        }
        else if (currentState == m_chaseStatePtr)
        {
            // Chase state: Orange
            shapeCircle.Color = new Color { R = 1.0f, G = 0.5f, B = 0.0f, A = 1.0f };
        }
        else
        {
            // Patrol state: Red with throb
            shapeCircle.Color = new Color { R = throb, G = 0.0f, B = 0.0f, A = 1.0f };
        }
    }

    private void UpdatePathDebugVisuals()
    {
        foreach (var entity in m_pathDebugEntities)
        {
            entity.Destroy();
        }
        m_pathDebugEntities.Clear();

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
                        Radius = isCurrent ? 8.0f : 5.0f,
                        Color = isCurrent
                            ? new Color { R = 0f, G = 1f, B = 0f, A = 0.8f }
                            : new Color { R = 1f, G = 1f, B = 0f, A = 0.5f },
                        Filled = true
                    }),
                    new ComponentData<Layer>(new() { Id = 0 }),
                    new ComponentData<Active>(new() { Enabled = true })
                );
                m_pathDebugEntities.Add(debugEntity);
            }
        }
    }

    public override void OnDestroy()
    {
        // Cleanup handled automatically by GrapeEngine
    }
}