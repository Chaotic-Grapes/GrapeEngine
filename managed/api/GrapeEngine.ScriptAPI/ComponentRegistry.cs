/* Start Header *****************************************************************/
/*!
\file   ComponentRegistry.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Component registration API for C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.ScriptAPI.Unsafe;
using System.Runtime.InteropServices;

namespace GrapeEngine;

/// <summary>
/// Component registration utilities for C# systems.
/// Use this to register C# component types with the native ECS.
/// </summary>
public static class ComponentRegistry
{
    private static readonly HashSet<uint> _registeredHashes = new();
    private static readonly object _lock = new();

    /// <summary>
    /// Register a component type with the native ECS.
    /// This must be called before using the component type in queries or entity operations.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged)</typeparam>
    /// <returns>True if registration succeeded, false if already registered</returns>
    public static bool Register<T>() where T : unmanaged
    {
        uint hash = ComponentTypeHelper.GetTypeHash<T>();
        int size = Marshal.SizeOf<T>();
        int alignment = GetAlignment<T>();

        lock (_lock)
        {
            // Check local cache first
            if (_registeredHashes.Contains(hash))
            {
                return false;
            }

            // Register with native code
            bool success = ComponentRegistryAPI.RegisterComponent(hash, size, alignment);
            
            if (success)
            {
                _registeredHashes.Add(hash);
                Console.WriteLine($"[ComponentRegistry] Registered {typeof(T).Name} (hash: 0x{hash:X8}, size: {size}, align: {alignment})");
            }

            return success;
        }
    }

    /// <summary>
    /// Check if a component type is registered.
    /// </summary>
    /// <typeparam name="T">Component type to check</typeparam>
    /// <returns>True if registered, false otherwise</returns>
    public static bool IsRegistered<T>() where T : unmanaged
    {
        uint hash = ComponentTypeHelper.GetTypeHash<T>();
        
        lock (_lock)
        {
            if (_registeredHashes.Contains(hash))
            {
                return true;
            }
        }

        // Check with native code in case it was registered in C++
        return ComponentRegistryAPI.IsComponentRegistered(hash);
    }

    /// <summary>
    /// Auto-register a component type if not already registered.
    /// This is called internally before component operations.
    /// </summary>
    internal static void EnsureRegistered<T>() where T : unmanaged
    {
        if (!IsRegistered<T>())
        {
            Register<T>();
        }
    }

    /// <summary>
    /// Get alignment requirement for a type.
    /// </summary>
    private static unsafe int GetAlignment<T>() where T : unmanaged
    {
        // C# doesn't expose alignment directly, so we infer from size
        // For simple types, alignment is usually the same as size (up to pointer size)
        int size = sizeof(T);
        
        // Common alignments: 1, 2, 4, 8, 16
        if (size >= 16) return 16;
        if (size >= 8) return 8;
        if (size >= 4) return 4;
        if (size >= 2) return 2;
        return 1;
    }
}
