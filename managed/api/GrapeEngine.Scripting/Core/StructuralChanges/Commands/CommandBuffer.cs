/* Start Header *****************************************************************/
/*!
\file   CommandBuffer.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Records structural changes and plays them back safely.

Deferred command buffer for safe entity/component modifications.
Records structural changes (add/remove entities, add/remove components)
and applies them at safe synchronization points to prevent data races.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core.StructuralChanges.Commands;

namespace GrapeEngine.Scripting.Core.StructuralChanges.Commands;

/// <summary>
/// Records structural changes and plays them back safely.
/// 
/// Allows queuing of structural modifications (create/destroy entities,
/// add/remove components) without causing data races. Changes are applied at a
/// safe synchronization point after all system updates complete.
/// 
/// Example:
/// <code>
/// var buffer = new CommandBuffer(world);
/// 
/// deadQuery.ForEachEntity((in Dead d) =>
/// {
///     buffer.DestroyEntity(entity);  // Recorded, not executed yet
/// });
/// 
/// buffer.Playback();  // Now execute all recorded commands
/// </code>
/// </summary>
/// <remarks>
/// Create a command buffer for deferred modifications.
/// </remarks>
/// <param name="world">The world to operate on</param>
public class CommandBuffer(World world) : IDisposable
{
    private readonly World _world = world ?? throw new ArgumentNullException(nameof(world));
    private readonly List<Command> _commands = [];
    private int _commandCount = 0;
    private bool _isRecording = true;
    private uint _frameIndex = 0;

    /// <summary>
    /// Internal accessor for the world reference.
    /// </summary>
    internal World World => _world;

    /// <summary>
    /// Record a command to create a new entity.
    /// </summary>
    /// <returns>Entity handle (not yet valid until Playback)</returns>
    public Entity CreateEntity()
    {
        if (!_isRecording)
            throw new InvalidOperationException("Command buffer is not recording");

        var command = new Command
        {
            Type = CommandType.CreateEntity,
            OrderIndex = _commandCount++,
            Frame = _frameIndex
        };

        _commands.Add(command);

        // Return temporary entity handle
        return Entity.FromId(_world, 0x80000000UL | (ulong)(_commandCount - 1));
    }

    /// <summary>
    /// Record a command to destroy an entity.
    /// </summary>
    /// <param name="entity">Entity to destroy</param>
    public void DestroyEntity(Entity entity)
    {
        if (!_isRecording)
            throw new InvalidOperationException("Command buffer is not recording");

        var command = new Command
        {
            Type = CommandType.DestroyEntity,
            TargetEntity = entity,
            OrderIndex = _commandCount++,
            Frame = _frameIndex
        };

        _commands.Add(command);
    }

    /// <summary>
    /// Record a command to add a component to an entity.
    /// </summary>
    /// <typeparam name="T">Component type to add</typeparam>
    /// <param name="entity">Entity to add component to</param>
    /// <param name="component">Component value</param>
    public void AddComponent<T>(Entity entity, T component) where T : unmanaged
    {
        if (!_isRecording)
            throw new InvalidOperationException("Command buffer is not recording");

        var hash = GetComponentTypeHash<T>();
        var command = new Command
        {
            Type = CommandType.AddComponent,
            TargetEntity = entity,
            ComponentTypeHash = hash,
            ComponentType = typeof(T),
            ComponentData = (object)component,
            OrderIndex = _commandCount++,
            Frame = _frameIndex
        };

        _commands.Add(command);
    }

    /// <summary>
    /// Record a command to remove a component from an entity.
    /// </summary>
    /// <typeparam name="T">Component type to remove</typeparam>
    /// <param name="entity">Entity to remove component from</param>
    public void RemoveComponent<T>(Entity entity) where T : unmanaged
    {
        if (!_isRecording)
            throw new InvalidOperationException("Command buffer is not recording");

        var hash = GetComponentTypeHash<T>();
        var command = new Command
        {
            Type = CommandType.RemoveComponent,
            TargetEntity = entity,
            ComponentTypeHash = hash,
            ComponentType = typeof(T),
            OrderIndex = _commandCount++,
            Frame = _frameIndex
        };

        _commands.Add(command);
    }

    /// <summary>
    /// Record a command to set a component value.
    /// </summary>
    /// <typeparam name="T">Component type to set</typeparam>
    /// <param name="entity">Entity to set component on</param>
    /// <param name="component">New component value</param>
    public void SetComponent<T>(Entity entity, T component) where T : unmanaged
    {
        if (!_isRecording)
            throw new InvalidOperationException("Command buffer is not recording");

        var hash = GetComponentTypeHash<T>();
        var command = new Command
        {
            Type = CommandType.SetComponent,
            TargetEntity = entity,
            ComponentTypeHash = hash,
            ComponentType = typeof(T),
            ComponentData = (object)component,
            OrderIndex = _commandCount++,
            Frame = _frameIndex
        };

        _commands.Add(command);
    }

    /// <summary>
    /// Record a command to instantiate an entity from an archetype.
    /// </summary>
    /// <param name="archetypeId">Archetype to instantiate from</param>
    /// <returns>New entity handle (valid after Playback)</returns>
    public Entity Instantiate(uint archetypeId)
    {
        if (!_isRecording)
            throw new InvalidOperationException("Command buffer is not recording");

        var command = new Command
        {
            Type = CommandType.InstantiateEntity,
            ComponentData = archetypeId,
            OrderIndex = _commandCount++,
            Frame = _frameIndex
        };

        _commands.Add(command);

        return Entity.FromId(_world, 0x80000000UL | (ulong)(_commandCount - 1));
    }

    /// <summary>
    /// Record a command to clone an existing entity.
    /// </summary>
    /// <param name="sourceEntity">Entity to clone</param>
    /// <returns>New entity handle (valid after Playback)</returns>
    public Entity Clone(Entity sourceEntity)
    {
        if (!_isRecording)
            throw new InvalidOperationException("Command buffer is not recording");

        var command = new Command
        {
            Type = CommandType.CloneEntity,
            TargetEntity = sourceEntity,
            OrderIndex = _commandCount++,
            Frame = _frameIndex
        };

        _commands.Add(command);

        return Entity.FromId(_world, 0x80000000UL | (ulong)(_commandCount - 1));
    }

    /// <summary>
    /// Execute all recorded commands on the world.
    /// 
    /// This should be called after all queries complete and before the next
    /// frame or system execution to ensure all structural changes are applied safely.
    /// </summary>
    /// <returns>Number of commands executed</returns>
    public int Playback()
    {
        if (_commands.Count == 0)
            return 0;

        var executedCount = 0;

        // Execute commands in order
        foreach (var command in _commands)
        {
            try
            {
                ExecuteCommand(command);
                executedCount++;
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"Error executing command {command.Type}: {ex.Message}", LogLevel.Error);
            }
        }

        _commands.Clear();
        _commandCount = 0;

        return executedCount;
    }

    /// <summary>
    /// Execute a single command on the world.
    /// </summary>
    private void ExecuteCommand(Command command)
    {
        switch (command.Type)
        {
            case CommandType.CreateEntity:
                _world.CreateEntity();
                break;

            case CommandType.DestroyEntity:
                _world.DestroyEntity(command.TargetEntity);
                break;

            case CommandType.AddComponent:
                ExecuteAddComponent(command);
                break;

            case CommandType.RemoveComponent:
                ExecuteRemoveComponent(command);
                break;

            case CommandType.SetComponent:
                ExecuteSetComponent(command);
                break;

            case CommandType.InstantiateEntity:
                _world.InstantiateEntity((uint)command.ComponentData);
                break;

            case CommandType.CloneEntity:
                _world.CloneEntity(command.TargetEntity);
                break;
        }
    }

    /// <summary>
    /// Execute AddComponent command via reflection-based generic invocation.
    /// </summary>
    private void ExecuteAddComponent(Command command)
    {
        if (command.ComponentType == null || command.ComponentData == null)
        {
            Logging.LogInternal("[CommandBuffer] AddComponent failed: Missing component type or data", LogLevel.Warning);
            return;
        }

        try
        {
            var method = typeof(World).GetMethod(
                "AddComponent",
                System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance,
                null,
                [typeof(Entity), command.ComponentType],
                null) ?? (typeof(World).GetMethod("AddComponent")?.MakeGenericMethod(command.ComponentType));

            if (method != null)
            {
                method.Invoke(_world, [command.TargetEntity, command.ComponentData]);
            }
            else
            {
                Logging.LogInternal($"[CommandBuffer] AddComponent failed: Could not find method for {command.ComponentType.Name}", LogLevel.Warning);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[CommandBuffer] AddComponent error: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Execute RemoveComponent command via reflection-based generic invocation.
    /// </summary>
    private void ExecuteRemoveComponent(Command command)
    {
        if (command.ComponentType == null)
        {
            Logging.LogInternal("[CommandBuffer] RemoveComponent failed: Missing component type", LogLevel.Warning);
            return;
        }

        try
        {
            var method = typeof(World).GetMethod("RemoveComponent")
                ?.MakeGenericMethod(command.ComponentType);

            if (method != null)
            {
                method.Invoke(_world, [command.TargetEntity]);
            }
            else
            {
                Logging.LogInternal($"[CommandBuffer] RemoveComponent failed: Could not find method for {command.ComponentType.Name}", LogLevel.Warning);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[CommandBuffer] RemoveComponent error: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Execute SetComponent command via reflection-based generic invocation.
    /// </summary>
    private void ExecuteSetComponent(Command command)
    {
        if (command.ComponentType == null || command.ComponentData == null)
        {
            Logging.LogInternal($"[CommandBuffer] SetComponent failed: Missing component type or data", LogLevel.Warning);
            return;
        }

        try
        {
            var method = typeof(World).GetMethod("SetComponent")?.MakeGenericMethod(command.ComponentType);

            if (method != null)
            {
                method.Invoke(_world, [command.TargetEntity, command.ComponentData]);
            }
            else
            {
                Logging.LogInternal($"[CommandBuffer] SetComponent failed: Could not find method for {command.ComponentType.Name}", LogLevel.Warning);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[CommandBuffer] SetComponent error: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Get count of pending commands.
    /// </summary>
    public int GetPendingCommandCount() => _commandCount;

    /// <summary>
    /// Clear all pending commands without executing.
    /// </summary>
    public void Clear()
    {
        _commands.Clear();
        _commandCount = 0;
    }

    /// <summary>
    /// Enable or disable command recording.
    /// </summary>
    public void SetRecording(bool recording)
    {
        _isRecording = recording;
    }

    /// <summary>
    /// Check if buffer is currently recording.
    /// </summary>
    public bool IsRecording => _isRecording;

    /// <summary>
    /// Set frame index for command tracking.
    /// </summary>
    public void SetFrame(uint frameIndex)
    {
        _frameIndex = frameIndex;
    }

    /// <summary>
    /// Get component type hash for hashing/matching.
    /// </summary>
    private static ulong GetComponentTypeHash<T>() where T : unmanaged
    {
        // FNV-1a hash of type name
        if (typeof(T).FullName is not string typeName)
            throw new InvalidOperationException("Type must have a full name");

        var hash = 14695981039346656037;

        foreach (var c in typeName)
        {
            hash ^= c;
            hash *= 1099511628211;
        }

        return hash;
    }

    /// <summary>
    /// Dispose and clean up resources.
    /// </summary>
    public void Dispose()
    {
        _commands.Clear();
        GC.SuppressFinalize(this);
    }
}

