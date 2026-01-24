/* Start Header *****************************************************************/
/*!
\file   CommandBufferExtensions.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Extension methods for integrating command buffers with job systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Commands;

/// <summary>
/// Extension methods for integrating command buffers with job systems.
/// </summary>
public static class CommandBufferExtensions
{
    /// <summary>
    /// Create a command buffer and execute jobs with deferred changes.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="action">Action that uses buffer</param>
    public static void ExecuteWithDeferredChanges(
        this World world,
        Action<CommandBuffer> action)
    {
        var buffer = new CommandBuffer(world);
        action(buffer);
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
