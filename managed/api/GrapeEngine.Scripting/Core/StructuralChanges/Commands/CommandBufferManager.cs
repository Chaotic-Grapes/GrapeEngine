/* Start Header *****************************************************************/
/*!
\file   CommandBufferManager.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Manager for multiple command buffers with synchronization.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Commands;

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

