/* Start Header *****************************************************************/
/*!
\file   SystemRegistry.cs
\brief  Thin facade over SystemDiscovery for registering managed systems.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Systems;

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// Simple registry facade exposing ergonomic registration APIs for managed systems.
/// Delegates to `SystemDiscovery` which remains the authoritative registry.
/// </summary>
public static class SystemRegistry
{
    /// <summary>
    /// Register a system type and create an instance. Returns the assigned handle.
    /// This will call into SystemDiscovery to allocate a handle and instantiate.
    /// </summary>
    public static ulong Register<TSystem>() where TSystem : ISystem, new()
    {
        return SystemDiscovery.CreateSystemInstanceFromType(typeof(TSystem));
    }

    /// <summary>
    /// Register an existing `ISystem` instance and return the assigned handle.
    /// </summary>
    public static ulong RegisterInstance(ISystem system)
    {
        return SystemDiscovery.RegisterInstance(system);
    }

    /// <summary>
    /// Get a previously registered system instance by handle.
    /// </summary>
    public static object? GetInstance(ulong handle) => SystemDiscovery.GetSystemInstance(handle);

    /// <summary>
    /// Clear all registered systems (delegates to discovery clear).
    /// </summary>
    public static void Clear() => SystemDiscovery.ClearDiscoveredSystems();
}


