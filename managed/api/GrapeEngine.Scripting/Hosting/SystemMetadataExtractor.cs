/* Start Header *****************************************************************/
/*!
\file   SystemMetadataExtractor.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Utility for extracting system metadata from attributes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Reflection;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// SYSTEM METADATA EXTRACTION - Reflection-based system configuration discovery.
/// 
/// SystemMetadataExtractor uses .NET reflection to analyze ISystem implementations
/// and extract configuration metadata from interfaces and attributes at runtime.
/// This allows systems to be configured declaratively without centralized registration.
/// 
/// REFLECTION PERFORMANCE:
/// - All GetCustomAttribute() calls use fast Type reflection (cached by runtime)
/// - Enumerating attributes: O(number of attributes on type)
/// - Generic attribute detection: Uses Type.IsGenericType and name matching
/// - Recommended: Call once during system initialization, cache results
/// 
/// METADATA PRIORITY:
/// System configuration is resolved in priority order:
/// 1. ISystemMetadata interface on instance (highest priority, most specific)
/// 2. [SystemGroup] attribute on type (declarative attribute)
/// 3. Default behavior (fallback - SystemGroup.Update)
/// 
/// This allows runtime overrides while maintaining sensible defaults.
/// 
/// ATTRIBUTE DETECTION:
/// - ReadOnly and WriteAccess attributes are detected by:
///   * Checking if type is generic
///   * Comparing generic definition name to "ReadOnlyAttribute`1" etc
///   * Extracting generic argument for component type
/// - This pattern works because attributes are applied generically but
///   reflection sees the closed generic types at runtime
/// 
/// SYSTEM PHASES (EXECUTION GROUPS):
/// Systems can execute in different phases to control order:
/// - Init: One-time initialization, before Update systems run
/// - Update: Main game logic phase
/// - LateUpdate: Post-processing, cleanup, deferred operations
/// - FixedUpdate: Physics systems, deterministic calculations
/// - Custom: User-defined phases for advanced control
/// 
/// EDIT MODE:
/// [ExecuteInEditMode] systems run when editor is paused (not in play mode).
/// Useful for editor-only systems (gizmos, debug visualization, prefab editing).
/// 
/// COMPONENT ACCESS ANNOTATIONS:
/// Systems can declare component access requirements:
/// - [ReadOnly<T>]: Component is read-only, concurrent reads allowed
/// - [WriteAccess<T>]: Component has write access, exclusive access required
/// - Helps validation and future parallelization
/// 
/// REFLECTION CACHING RECOMMENDATION:
/// Extract metadata once during system registration, not per frame.
/// Example:
/// ```csharp
/// // Bad (per-frame)
/// var group = SystemMetadataExtractor.GetSystemGroup(systemType);
/// 
/// // Good (once during init)
/// var systemConfig = new SystemConfig 
/// { 
///     Group = SystemMetadataExtractor.GetSystemGroup(systemType),
///     ReadOnlyComponents = SystemMetadataExtractor.GetReadOnlyComponents(systemType).ToList()
/// };
/// ```
/// </summary>
public static class SystemMetadataExtractor
{
    /// <summary>
    /// Get the execution group (phase) a system runs in.
    /// Priority: ISystemMetadata interface > [SystemGroup] attribute > default (SystemGroup.Update)
    /// </summary>
    public static SystemGroup GetSystemGroup(Type systemType, object? instance = null)
    {
        // Priority 1: ISystemMetadata interface on instance
        if (instance is ISystemMetadata metadata)
        {
            return metadata.Group;
        }

        // Priority 2: [SystemGroup] attribute
        var attr = systemType.GetCustomAttribute<SystemGroupAttribute>();
        if (attr != null)
        {
            return attr.Group;
        }

        // Default
        return SystemGroup.Update;
    }

    /// <summary>
    /// Check if a system type has the [ExecuteInEditMode] attribute.
    /// </summary>
    public static bool HasExecuteInEditMode(Type systemType)
    {
        return systemType.GetCustomAttribute<ExecuteInEditModeAttribute>() != null;
    }

    /// <summary>
    /// Get all component types marked as read-only for a system.
    /// </summary>
    public static IEnumerable<Type> GetReadOnlyComponents(Type systemType)
    {
        var attrs = systemType.GetCustomAttributes();
        foreach (var attr in attrs)
        {
            // Check for ReadOnlyAttribute<T>
            if (attr.GetType().IsGenericType && 
                attr.GetType().GetGenericTypeDefinition().Name == "ReadOnlyAttribute`1")
            {
                yield return attr.GetType().GetGenericArguments()[0];
            }
        }
    }

    /// <summary>
    /// Get all component types marked as having write access for a system.
    /// </summary>
    public static IEnumerable<Type> GetWriteAccessComponents(Type systemType)
    {
        var attrs = systemType.GetCustomAttributes();
        foreach (var attr in attrs)
        {
            // Check for WriteAccessAttribute<T>
            if (attr.GetType().IsGenericType && 
                attr.GetType().GetGenericTypeDefinition().Name == "WriteAccessAttribute`1")
            {
                yield return attr.GetType().GetGenericArguments()[0];
            }
        }
    }

    /// <summary>
    /// Get all component access declarations for a system with their modes.
    /// Returns tuples of (ComponentType, AccessMode).
    /// </summary>
    public static IEnumerable<(Type ComponentType, ComponentAccessMode Mode)> GetComponentAccesses(Type systemType)
    {
        var attrs = systemType.GetCustomAttributes();
        foreach (var attr in attrs)
        {
            if (!attr.GetType().IsGenericType)
                continue;

            var genericDef = attr.GetType().GetGenericTypeDefinition().Name;
            var componentType = attr.GetType().GetGenericArguments()[0];

            if (genericDef == "ReadOnlyAttribute`1")
            {
                yield return (componentType, ComponentAccessMode.Read);
            }
            else if (genericDef == "WriteAccessAttribute`1")
            {
                yield return (componentType, ComponentAccessMode.Write);
            }
        }
    }

    /// <summary>
    /// Get access mode for a specific component type in a system.
    /// </summary>
    public static ComponentAccessMode GetComponentAccessMode(Type systemType, Type componentType)
    {
        var componentHash = componentType.GetHashCode();
        
        foreach (var (compType, mode) in GetComponentAccesses(systemType))
        {
            if (compType.GetHashCode() == componentHash)
            {
                return mode;
            }
        }

        // Default: assume read-only if not declared
        return ComponentAccessMode.Read;
    }

    /// <summary>
    /// Check if a system declares read-only access for a component.
    /// </summary>
    public static bool HasReadOnlyAccess(Type systemType, Type componentType)
    {
        return GetComponentAccessMode(systemType, componentType) == ComponentAccessMode.Read;
    }

    /// <summary>
    /// Check if a system declares write access for a component.
    /// </summary>
    public static bool HasWriteAccess(Type systemType, Type componentType)
    {
        var mode = GetComponentAccessMode(systemType, componentType);
        return mode == ComponentAccessMode.Write || mode == ComponentAccessMode.ReadWrite;
    }

    /// <summary>
    /// Get the system name (type name if no custom name is set).
    /// </summary>
    public static string GetSystemName(Type systemType)
    {
        return systemType.Name;
    }

    /// <summary>
    /// Get the fully qualified system name.
    /// </summary>
    public static string GetSystemFullName(Type systemType)
    {
        return systemType.FullName ?? systemType.Name;
    }

    /// <summary>
    /// Get the assembly name containing the system.
    /// </summary>
    public static string GetAssemblyName(Type systemType)
    {
        return systemType.Assembly.GetName().Name ?? "Unknown";
    }
}
