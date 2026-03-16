/* Start Header *****************************************************************/
/*!
\file   SystemDiscovery.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Discovers and manages ISystem implementations via reflection.
*/
/* End Header *******************************************************************/

using System.Reflection;
using GrapeEngine.Scripting.Systems;

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// SystemDiscovery - Encapsulates system discovery and instantiation logic.
/// 
/// Responsibilities:
/// - Discover ISystem implementations in loaded assemblies using reflection
/// - Instantiate system objects and track them
/// - Map system types to handles for C++ interop
/// - Manage system lifecycle and hot reload state
/// </summary>
internal static partial class SystemDiscovery
{
    /// <summary>
    /// Discovered system types, mapped to unique handles.
    /// Key: Handle (opaque identifier for C++)
    /// Value: Type that implements ISystem
    /// </summary>
    private static readonly Dictionary<ulong, Type> _systemTypes = [];
    private static readonly Dictionary<Type, ulong> _handlesByType = [];

    /// <summary>
    /// Instantiated system objects.
    /// Key: Handle
    /// Value: System instance
    /// </summary>
    private static readonly Dictionary<ulong, object> _systemInstances = [];
    private static readonly object _sync = new();

    private static bool IsRecoverableSystemException(Exception ex)
    {
        return ex is ReflectionTypeLoadException
            or TypeLoadException
            or InvalidOperationException
            or MissingMethodException
            or MemberAccessException
            or TargetInvocationException
            or ArgumentException
            or NotSupportedException;
    }

    /// <summary>
    /// Next available system handle.
    /// </summary>
    private static ulong _nextSystemHandle = 1;
}


