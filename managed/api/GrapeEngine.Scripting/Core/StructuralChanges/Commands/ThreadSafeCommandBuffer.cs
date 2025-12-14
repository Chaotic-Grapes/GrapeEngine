/* Start Header *****************************************************************/
/*!
\file   ThreadSafeCommandBuffer.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Thread-safe command buffer for use in parallel jobs.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Commands;

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
