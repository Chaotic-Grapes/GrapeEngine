using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

/// <summary>
/// EntityManager - Orchestrates the entire scripting test scene from C#.
/// Handles spawning all entities, world boundaries, and test logic.
/// This demonstrates that complex scene setup can be entirely script-driven.
/// </summary>
public class EntityManager : ScriptBehaviour
{
    // Entity references for spawned objects
    private Entity m_player;
    private Entity m_enemy1;
    private Entity m_enemy2;
    private Entity m_collectible1;
    private Entity m_collectible2;

    // Controller entities (separate from visuals)
    private Entity m_playerController;
    private Entity m_enemy1Controller;
    private Entity m_enemy2Controller;
    private Entity m_collectible1Controller;
    private Entity m_collectible2Controller;

    // World boundary entities
    private Entity m_boundaryTop;
    private Entity m_boundaryBottom;
    private Entity m_boundaryLeft;
    private Entity m_boundaryRight;

    public override void OnStart()
    {
        Log("=== EntityManager ===", LogLevel.Info);
        
        // Create world boundaries first
        CreateWorldBoundaries();
        
        // Spawn all test entities
        SpawnPlayer();
        SpawnEnemies();
        SpawnCollectibles();

        Log($"Created {5} controller entities + {5} visual entities + {4} boundary entities", LogLevel.Info);
        Log("Player: Green circle with figure-8 movement", LogLevel.Info);
        Log("Enemies: 2 red circles with patrol behavior", LogLevel.Info);
        Log("Collectibles: 2 rainbow circles with bobbing motion", LogLevel.Info);
    }

    public override void OnUpdate()
    {
        // Could add scene management logic here
        // For example: respawning destroyed entities, managing waves, etc.
    }

    private void CreateWorldBoundaries()
    {
        Log("Creating world boundaries...", LogLevel.Info);

        // Top wall
        m_boundaryTop = CreateEntity();
        m_boundaryTop.SetComponent(new LocalTransform
        {
            Position = new Vector3(World.Width / 2, World.WallThickness / 2, 0),
            Rotation = Quaternion.Identity,
            Scale = new Vector3(1, 1, 1)
        });
        m_boundaryTop.SetComponent(new ShapeBox2D
        {
            HalfExtents = new Vector2(World.Width / 2, World.WallThickness / 2),
            Offset = Vector2.Zero,
            Color = new Color { R = 0.3f, G = 0.3f, B = 0.3f, A = 1.0f },
            Filled = true
        });
        m_boundaryTop.SetComponent(new Layer { Id = 0 }); // Default layer
        m_boundaryTop.SetComponent(new Active { Enabled = true });

        // Bottom wall
        m_boundaryBottom = CreateEntity();
        m_boundaryBottom.SetComponent(new LocalTransform
        {
            Position = new Vector3(World.Width / 2, World.Height - World.WallThickness / 2, 0),
            Rotation = Quaternion.Identity,
            Scale = new Vector3(1, 1, 1)
        });
        m_boundaryBottom.SetComponent(new ShapeBox2D
        {
            HalfExtents = new Vector2(World.Width / 2, World.WallThickness / 2),
            Offset = Vector2.Zero,
            Color = new Color { R = 0.3f, G = 0.3f, B = 0.3f, A = 1.0f },
            Filled = true
        });
        m_boundaryBottom.SetComponent(new Layer { Id = 0 });
        m_boundaryBottom.SetComponent(new Active { Enabled = true });

        // Left wall
        m_boundaryLeft = CreateEntity();
        m_boundaryLeft.SetComponent(new LocalTransform
        {
            Position = new Vector3(World.WallThickness / 2, World.Height / 2, 0),
            Rotation = Quaternion.Identity,
            Scale = new Vector3(1, 1, 1)
        });
        m_boundaryLeft.SetComponent(new ShapeBox2D
        {
            HalfExtents = new Vector2(World.WallThickness / 2, World.Height / 2),
            Offset = Vector2.Zero,
            Color = new Color { R = 0.3f, G = 0.3f, B = 0.3f, A = 1.0f },
            Filled = true
        });
        m_boundaryLeft.SetComponent(new Layer { Id = 0 });
        m_boundaryLeft.SetComponent(new Active { Enabled = true });

        // Right wall
        m_boundaryRight = CreateEntity();
        m_boundaryRight.SetComponent(new LocalTransform
        {
            Position = new Vector3(World.Width - World.WallThickness / 2, World.Height / 2, 0),
            Rotation = Quaternion.Identity,
            Scale = new Vector3(1, 1, 1)
        });
        m_boundaryRight.SetComponent(new ShapeBox2D
        {
            HalfExtents = new Vector2(World.WallThickness / 2, World.Height / 2),
            Offset = Vector2.Zero,
            Color = new Color { R = 0.3f, G = 0.3f, B = 0.3f, A = 1.0f },
            Filled = true
        });
        m_boundaryRight.SetComponent(new Layer { Id = 0 });
        m_boundaryRight.SetComponent(new Active { Enabled = true });

        Log("World boundaries created (4 walls)", LogLevel.Info);
    }

    private void SpawnPlayer()
    {
        Log("Spawning Player...", LogLevel.Info);

        // Create a controller entity for the PlayerController script
        m_playerController = CreateEntity();
        m_playerController.SetComponent(new Active { Enabled = true });
        
        // The PlayerController script will create its own visual entity
        // We just need to attach the script component
        // Note: In a full implementation, we'd have a way to attach scripts via C# API
        // For now, the C++ side will attach this after we create it
    }

    private void SpawnEnemies()
    {
        Log("Spawning Enemies...", LogLevel.Info);

        // Enemy 1 - left side
        m_enemy1Controller = CreateEntity();
        m_enemy1Controller.SetComponent(new Active { Enabled = true });
        
        // Enemy 2 - right side  
        m_enemy2Controller = CreateEntity();
        m_enemy2Controller.SetComponent(new Active { Enabled = true });
    }

    private void SpawnCollectibles()
    {
        Log("Spawning Collectibles...", LogLevel.Info);

        // Collectible 1
        m_collectible1Controller = CreateEntity();
        m_collectible1Controller.SetComponent(new Active { Enabled = true });

        // Collectible 2
        m_collectible2Controller = CreateEntity();
        m_collectible2Controller.SetComponent(new Active { Enabled = true });
    }

    public override void OnDestroy()
    {
        Log("EntityManager: Cleaning up scene...", LogLevel.Info);
        
        // Cleanup is automatic when entities are destroyed
        // but we could manually clean up here if needed
    }
}
