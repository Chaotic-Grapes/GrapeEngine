/* Start Header *****************************************************************/
/*!
\file   CommandBuffer.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Deferred command buffer for safe entity/component modifications from jobs.

Records structural changes (add/remove entities, add/remove components)
and applies them at safe synchronization points to prevent data races
during parallel job execution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Job;

namespace GrapeEngine.Scripting;

/// <summary>
/// Type of structural change command.
/// </summary>
public enum CommandType
{
    /// <summary>
    /// Create a new entity
    /// </summary>
    CreateEntity,

    /// <summary>
    /// Destroy an entity
    /// </summary>
    DestroyEntity,

    /// <summary>
    /// Add component to entity
    /// </summary>
    AddComponent,

    /// <summary>
    /// Remove component from entity
    /// </summary>
    RemoveComponent,

    /// <summary>
    /// Set component value on entity
    /// </summary>
    SetComponent,

    /// <summary>
    /// Instantiate entity from archetype
    /// </summary>
    InstantiateEntity,

    /// <summary>
    /// Clone existing entity
    /// </summary>
    CloneEntity
}

/// <summary>
/// Represents a deferred structural change command.
/// </summary>
public struct Command
{
    /// <summary>
    /// Type of command to execute
    /// </summary>
    public CommandType Type { get; set; }

    /// <summary>
    /// Target entity (for add/remove/set operations)
    /// </summary>
    public Entity TargetEntity { get; set; }

    /// <summary>
    /// Component type hash (for add/remove/set operations)
    /// </summary>
    public ulong ComponentTypeHash { get; set; }

    /// <summary>
    /// Component runtime type (for generic method invocation)
    /// </summary>
    public Type ComponentType { get; set; }

    /// <summary>
    /// Component data (for set operations)
    /// </summary>
    public object ComponentData { get; set; }

    /// <summary>
    /// User-defined metadata for the command
    /// </summary>
    public object Metadata { get; set; }

    /// <summary>
    /// Frame this command was recorded
    /// </summary>
    public uint Frame { get; set; }

    /// <summary>
    /// Playback order index
    /// </summary>
    public int OrderIndex { get; set; }
}

/// <summary>
/// Records structural changes and plays them back safely.
/// 
/// Allows parallel jobs to queue structural modifications (create/destroy entities,
/// add/remove components) without causing data races. Changes are applied at a
/// safe synchronization point after all jobs complete.
/// 
/// Example:
/// <code>
/// public JobHandle OnUpdateAsJobs(World world, float deltaTime)
/// {
///     var buffer = new CommandBuffer(world);
///     
///     var handle = deadQuery.ForEachEntity((in Dead d) =>
///     {
///         buffer.DestroyEntity(entity);  // Recorded, not executed yet
///     });
///     
///     handle.Complete();
///     buffer.Playback();  // Now execute all recorded commands
/// }
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
    /// This should be called after all jobs complete and before the next
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
                Console.WriteLine($"Error executing command {command.Type}: {ex.Message}");
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
            Console.WriteLine("[CommandBuffer] AddComponent failed: Missing component type or data");
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
                Console.WriteLine(
                    $"[CommandBuffer] AddComponent failed: Could not find method for {command.ComponentType.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CommandBuffer] AddComponent error: {ex.Message}");
        }
    }

    /// <summary>
    /// Execute RemoveComponent command via reflection-based generic invocation.
    /// </summary>
    private void ExecuteRemoveComponent(Command command)
    {
        if (command.ComponentType == null)
        {
            Console.WriteLine("[CommandBuffer] RemoveComponent failed: Missing component type");
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
                Console.WriteLine($"[CommandBuffer] RemoveComponent failed: Could not find method for {command.ComponentType.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CommandBuffer] RemoveComponent error: {ex.Message}");
        }
    }

    /// <summary>
    /// Execute SetComponent command via reflection-based generic invocation.
    /// </summary>
    private void ExecuteSetComponent(Command command)
    {
        if (command.ComponentType == null || command.ComponentData == null)
        {
            Console.WriteLine($"[CommandBuffer] SetComponent failed: Missing component type or data");
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
                Console.WriteLine($"[CommandBuffer] SetComponent failed: Could not find method for {command.ComponentType.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[CommandBuffer] SetComponent error: {ex.Message}");
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

/// <summary>
/// Manager for multiple command buffers with synchronization.
/// 
/// Coordinates multiple command buffers across systems and jobs,
/// ensuring they're all executed at safe synchronization points.
/// </summary>
/// <remarks>
/// Create a command buffer manager for a world.
/// </remarks>
public class CommandBufferManager(World world)
{
    private readonly World _world = world ?? throw new ArgumentNullException(nameof(world));
    private readonly Dictionary<string, CommandBuffer> _buffers = [];
    private readonly List<CommandBuffer> _playbackQueue = [];
    private uint _frameIndex = 0;

    /// <summary>
    /// Create or get a named command buffer.
    /// </summary>
    /// <param name="name">Unique buffer name</param>
    /// <returns>Command buffer instance</returns>
    public CommandBuffer GetOrCreateBuffer(string name)
    {
        if (!_buffers.TryGetValue(name, out CommandBuffer? value))
        {
            var buffer = new CommandBuffer(_world);
            buffer.SetFrame(_frameIndex);
            value = buffer;
            _buffers[name] = value;
        }

        return value;
    }

    /// <summary>
    /// Register a buffer for playback at frame end.
    /// </summary>
    public void RegisterForPlayback(CommandBuffer buffer)
    {
        if (!_playbackQueue.Contains(buffer))
        {
            _playbackQueue.Add(buffer);
        }
    }

    /// <summary>
    /// Play back all registered buffers.
    /// </summary>
    /// <returns>Total commands executed</returns>
    public int PlaybackAll()
    {
        var totalExecuted = 0;

        foreach (var buffer in _playbackQueue)
        {
            totalExecuted += buffer.Playback();
        }

        _playbackQueue.Clear();
        _frameIndex++;

        return totalExecuted;
    }

    /// <summary>
    /// Clear all buffers.
    /// </summary>
    public void ClearAll()
    {
        foreach (var buffer in _buffers.Values)
        {
            buffer.Clear();
        }

        _playbackQueue.Clear();
    }

    /// <summary>
    /// Get current frame index.
    /// </summary>
    public uint GetFrameIndex() => _frameIndex;

    /// <summary>
    /// Get buffer count.
    /// </summary>
    public int GetBufferCount() => _buffers.Count;
}

/// <summary>
/// Thread-safe command buffer for use in parallel jobs.
/// 
/// Uses locking to ensure thread-safe recording of commands from
/// multiple job threads.
/// </summary>
/// <remarks>
/// Create a thread-safe command buffer.
/// </remarks>
public class ThreadSafeCommandBuffer(World world)
{
    private readonly CommandBuffer _buffer = new(world);
    private readonly Lock _lock = new();

  /// <summary>
  /// Thread-safely destroy an entity.
  /// </summary>
  public void DestroyEntity(Entity entity)
    {
        lock (_lock)
        {
            _buffer.DestroyEntity(entity);
        }
    }

    /// <summary>
    /// Thread-safely add a component.
    /// </summary>
    public void AddComponent<T>(Entity entity, T component) where T : unmanaged
    {
        lock (_lock)
        {
            _buffer.AddComponent(entity, component);
        }
    }

    /// <summary>
    /// Thread-safely remove a component.
    /// </summary>
    public void RemoveComponent<T>(Entity entity) where T : unmanaged
    {
        lock (_lock)
        {
            _buffer.RemoveComponent<T>(entity);
        }
    }

    /// <summary>
    /// Playback all commands.
    /// </summary>
    public int Playback()
    {
        lock (_lock)
        {
            return _buffer.Playback();
        }
    }

    /// <summary>
    /// Get pending command count.
    /// </summary>
    public int GetPendingCommandCount()
    {
        lock (_lock)
        {
            return _buffer.GetPendingCommandCount();
        }
    }
}

/// <summary>
/// Extension methods for integrating command buffers with job systems.
/// </summary>
public static class CommandBufferExtensions
{
    /// <summary>
    /// Create a command buffer and execute jobs with deferred changes.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="jobAction">Action that schedules jobs and uses buffer</param>
    /// <returns>Final job handle</returns>
    public static JobHandle ExecuteWithDeferredChanges(
        this World world,
        Func<CommandBuffer, JobHandle> jobAction)
    {
        var buffer = new CommandBuffer(world);
        var handle = jobAction(buffer);

        // Note: Playback should happen after handle.Complete()
        // This is just scheduling the handle
        return handle;
    }

    /// <summary>
    /// Convert command buffer to thread-safe version.
    /// </summary>
    public static ThreadSafeCommandBuffer AsThreadSafe(this CommandBuffer buffer)
    {
        return buffer == null 
            ? throw new ArgumentNullException(nameof(buffer))
            : new ThreadSafeCommandBuffer(buffer.World);
  }
}
