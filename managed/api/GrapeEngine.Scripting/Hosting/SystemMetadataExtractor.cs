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

using System.Reflection;
using GrapeEngine.Scripting.Systems.Attributes;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// Extracts metadata from ISystem implementations using reflection.
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
