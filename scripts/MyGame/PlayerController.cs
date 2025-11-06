/**
* @Name: Dalton koh, 2403250
* @email: d.koh@digipen.edu
* @file PlayerController.cs
* @brief Keyboard-controlled green circle with rotation and pulsing visuals.
*
* @details
* Demonstrates basic input-driven movement in GrapeEngine:
* - WASD for movement, Q/E for rotation around Z (if we going to make it underwwater like but not just yet)
* - Clamps motion inside world boundaries
* - Pulsing radius/brightness for active visual feedback
* Provides a static Instance getter so other scripts (EnemyAI) can query the
* player's visual entity.
*
* @dependencies
* - GrapeEngine (Log, World, Time, Input)
* - GrapeEngine.Numerics (Vector2/3, Quaternion, Color)
* - GrapeEngine.Scripting (ScriptBehaviour, Entity, ComponentData<>)
* 
* Copyright (C) 2025 DigiPen Institute of Technology.
* Reproduction or disclosure of this file or its contents without the
*/

using GrapeEngine;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame;

/// <summary>
/// Player controller script to demonstrate keyboard input and movement.
/// </summary>
public class PlayerController : ScriptBehaviour
{
    //create static instance container to be get by other class 
    public static PlayerController Instance { get; private set; }

    // Movement settings
    private readonly float m_moveSpeed = 200.0f;
    private float m_rotationSpeed = 180.0f;

    // Visual entity that represents the player
    private Entity m_visualEntity;

    public override void OnStart()
    {
        // Set the static Instance property to point to this specific PlayerController object
        // This allows other classes to access this PlayerController via PlayerController.Instance
        Instance = this;
        
        Log("PlayerController initialized!", LogLevel.Info);
        Log("Use WASD to move, Q/E to rotate", LogLevel.Info);

        // Create the entity for the player
        m_visualEntity = CreateEntity(
            new ComponentData<LocalTransform>(new()
            {
                Position = new Vector3(World.Width * 0.5f, World.Height * 0.5f, 0.0f),
                Rotation = Quaternion.Identity,
                Scale = new Vector3(1, 1, 1)
            }),
            new ComponentData<ShapeCircle2D>(new()
            {
                Radius = 20.0f,
                Color = new Color { R = 0.0f, G = 1.0f, B = 0.0f, A = 1.0f }, // Green color
                Filled = true
            }),
            new ComponentData<Layer>(new() { Id = 0 }),
            new ComponentData<Active>(new() { Enabled = true })
        );

        Log($"Player visual entity created: {m_visualEntity.EntityId}", LogLevel.Info);
    }

    public override void OnUpdate()
    {
        // get transform of player
        ref var transform = ref m_visualEntity.TryGetComponent<LocalTransform>(out var Transform);

        // player has no transform log problem
        if (!Transform)
        { 
            Log("PlayerController: No LocalTransform component on visual entity!", LogLevel.Warning);
            return;
        }


        // WASD movement with keyboard input
        var movement = Vector2.Zero;

        if (Input.IsKeyDown(KeyCode.W))
            movement.Y += 1.0f; // Up
        if (Input.IsKeyDown(KeyCode.S))
            movement.Y -= 1.0f; // Down
        if (Input.IsKeyDown(KeyCode.A))
            movement.X -= 1.0f; // Left
        if (Input.IsKeyDown(KeyCode.D))
            movement.X += 1.0f; // Right

        // Normalize diagonal movement so we don't move faster diagonally
        if (movement.Magnitude > 0.0f)
        {
            movement = movement.Normalized;
            transform.Position.X += movement.X * m_moveSpeed * Time.DeltaTime;
            transform.Position.Y += movement.Y * m_moveSpeed * Time.DeltaTime;
        }

        // Rotation with Q/E keys
        if (Input.IsKeyDown(KeyCode.Q))
        {
            // Rotate counter-clockwise (increase Z rotation)
            var angle = m_rotationSpeed * Time.DeltaTime * (MathF.PI / 180.0f);
            var currentAngle = 2.0f * MathF.Acos(transform.Rotation.W);
            var newAngle = currentAngle + angle;
            transform.Rotation.W = MathF.Cos(newAngle / 2.0f);
            transform.Rotation.Z = MathF.Sin(newAngle / 2.0f);
        }
        if (Input.IsKeyDown(KeyCode.E))
        {
            // Rotate clockwise (decrease Z rotation)
            var angle = m_rotationSpeed * Time.DeltaTime * (MathF.PI / 180.0f);
            var currentAngle = 2.0f * MathF.Acos(transform.Rotation.W);
            var newAngle = currentAngle - angle;
            transform.Rotation.W = MathF.Cos(newAngle / 2.0f);
            transform.Rotation.Z = MathF.Sin(newAngle / 2.0f);
        }

        // Clamp to world bounds
        transform.Position.X = Math.Clamp(transform.Position.X, World.WallThickness, World.Width - World.WallThickness);
        transform.Position.Y = Math.Clamp(transform.Position.Y, World.WallThickness, World.Height - World.WallThickness);

        // Update visual feedback
        UpdateVisual();
    }

    private void UpdateVisual()
    {
        // Make the player pulse to show it's active
        ref var shapeCircle = ref m_visualEntity.TryGetComponent<ShapeCircle2D>(out var Circle);
        if (!Circle) return;


        var pulse = 0.9f + 0.1f * MathF.Sin((float)Time.ElapsedTime * 5.0f);
        shapeCircle.Radius = 20.0f * pulse;

        // Keep green color but change brightness
        shapeCircle.Color.R = 0.0f;
        shapeCircle.Color.G = pulse;
        shapeCircle.Color.B = 0.0f;
        shapeCircle.Color.A = 1.0f;
    }

    //getter for visual entity (so EnemyAI can access it)
    public Entity GetVisualEntity()
    {
        return m_visualEntity;
    }

    public override void OnFixedUpdate()
    {
        // Physics-based updates would go here if needed but not used in this demo
    }

    //kill character
    public override void OnDestroy()
    {
        // Clear static instance when destroyed
        if (Instance == this)
        {
            Instance = null;
        }
    }
}
