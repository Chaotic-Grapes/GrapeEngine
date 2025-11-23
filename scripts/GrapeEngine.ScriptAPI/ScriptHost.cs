/* Start Header *****************************************************************/
/*!
\file   ScriptHost.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th October 2025
\brief
The entry point for C++ to interact with the managed scripting system.
This class manages script instance lifecycle and dispatches calls.

\details
This class is responsible for creating, updating, and destroying script instances.
It also handles communication between C++ and C# scripts, allowing for seamless 
integration of managed code into the game engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

// *************** !!!!!!!! IMPORTANT !!!!!!!! *************** //
//                                                             //
// Please consult me before making ANY changes to this file.   //
// Unless it's just logging changes or documentation updates.  //
//                                                             //
// *************** !!!!!!!! IMPORTANT !!!!!!!! *************** //

namespace GrapeEngine.Scripting;

/// <summary>
/// The entry point for C++ to interact with the managed scripting system.
/// This class manages script instance lifecycle and dispatches calls.
/// </summary>
public static class ScriptHost
{
    // Store all active script instances
    private static readonly Dictionary<ulong, ScriptBehaviour> m_instances = new();
    private static ulong m_nextHandle = 1;
    
    // Custom assembly load context to ensure proper type identity
    private static AssemblyLoadContext? s_loadContext = null;
    private static bool s_resolverRegistered = false;

    /// <summary>
    /// Create a new script instance.
    /// Called from C++ when attaching a script to an entity.
    /// </summary>
    /// <param name="typeNamePtr">Pointer to C-string containing fully qualified type name</param>
    /// <param name="entityId">The entity ID this script is attached to</param>
    /// <returns>Handle to the created instance, or 0 on failure</returns>

    // CallConvs specifies the calling convention for this method.
    // In this case, we use Cdecl to match the C++ side. (matching is very important)
    // This ensures that the stack is cleaned up correctly after the function call.
    // Also take note: I encapsulated the typeof(...) in square brackets because it is an array.
    // Also PSA: I did not read much on interoperability with C++ so there are gonna be some bugs
    // All I know is that Cdecl is mostly used from my experience :)
    // UnmanagedCallersOnly attribute indicates that this method can be called from unmanaged code (C++ in this case).
    // It also prevents this method from being called from managed code directly.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])] // CallConvCdecl is in System.Runtime.CompilerServices
    public static ulong CreateScriptInstance(IntPtr typeNamePtr, ulong entityId)
    {
        try
        {
            // Marshal type name from C++
            // PtrToStringAnsi converts a pointer to an ANSI string (C-style null-terminated string)
            var typeName = Marshal.PtrToStringAnsi(typeNamePtr);

            // string.IsNullOrWhiteSpace is better than IsNullOrEmpty in most cases
            if (string.IsNullOrWhiteSpace(typeName))
            {
                Logging.Log("ERROR: Null or empty type name", LogLevel.Error);
                return 0;
            }

            Logging.Log($"Creating script instance: {typeName} for entity {entityId}", LogLevel.Info);

            // Find the type
            var scriptType = Type.GetType(typeName);
            if (scriptType == null)
            {
                // Try searching all loaded assemblies
                foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
                {
                    scriptType = assembly.GetType(typeName);
                    if (scriptType != null)
                        break;
                }
            }

            if (scriptType == null)
            {
                Logging.Log($"ERROR: Type not found: {typeName}", LogLevel.Error);
                return 0;
            }

            Logging.Log($"Found type: {scriptType.FullName}", LogLevel.Info);
            Logging.Log($"Base type: {scriptType.BaseType?.FullName ?? "null"}", LogLevel.Info);

            // Verify it's a ScriptBehaviour
            // Use name-based check to avoid assembly context issues
            bool isScriptBehaviour = false;
            var baseType = scriptType.BaseType;
            while (baseType != null)
            {
                if (baseType.Name == "ScriptBehaviour" || baseType.Name == nameof(ScriptBehaviour))
                {
                    isScriptBehaviour = true;
                    break;
                }
                baseType = baseType.BaseType;
            }

            if (!isScriptBehaviour)
            {
                Logging.Log($"ERROR: Type {typeName} does not inherit from ScriptBehaviour" + Environment.NewLine +
                            $"  Found in assembly: {scriptType.Assembly.FullName}" + Environment.NewLine +
                            $"  Type's full name: {scriptType.FullName}" + Environment.NewLine +
                            $"  Type's namespace: {scriptType.Namespace ?? "none"}" + Environment.NewLine +
                            $"  Type's name: {scriptType.Name}" + Environment.NewLine +
                            $"  Type's assembly: {scriptType.Assembly.FullName}" + Environment.NewLine +
                            $"  Type's module: {scriptType.Module.Name}" + Environment.NewLine +
                            $"  Type's attributes: {scriptType.Attributes}" + Environment.NewLine +
                            $"  Is class: {scriptType.IsClass}" + Environment.NewLine +
                            $"  Is public: {scriptType.IsPublic}" + Environment.NewLine +
                            $"  Is abstract: {scriptType.IsAbstract}" + Environment.NewLine +
                            $"  Is sealed: {scriptType.IsSealed}" + Environment.NewLine +
                            $"  Is generic type: {scriptType.IsGenericType}" + Environment.NewLine +
                            $"  Is nested: {scriptType.IsNested}" + Environment.NewLine +
                            $"  Is value type: {scriptType.IsValueType}" + Environment.NewLine +
                            $"  Is interface: {scriptType.IsInterface}" + Environment.NewLine +
                            $"  Is enum: {scriptType.IsEnum}" + Environment.NewLine +
                            $"  Is array: {scriptType.IsArray}" + Environment.NewLine +
                            $"  Is pointer: {scriptType.IsPointer}" + Environment.NewLine +
                            $"  Is primitive: {scriptType.IsPrimitive}" + Environment.NewLine +
                            $"  Type's full hierarchy:", LogLevel.Error);
                return 0;
            }

            // Create instance using reflection to avoid type identity issues
            var instance = Activator.CreateInstance(scriptType)!;
            
            // Set EntityId using reflection (avoids casting issues across assembly contexts)
            var entityIdProp = scriptType.GetProperty("EntityId", 
                System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
            entityIdProp?.SetValue(instance, entityId);

            // Try to cast to ScriptBehaviour
            // If this fails due to assembly context issues, we need to use reflection-based wrapper
            ScriptBehaviour? scriptBehaviour = instance as ScriptBehaviour;
            
            if (scriptBehaviour == null)
            {
                // Assembly context mismatch - the loaded type's ScriptBehaviour is different from ours
                Logging.Log($"ERROR: Type identity mismatch for {typeName}" + Environment.NewLine +
                            $"  Instance type: {instance.GetType().FullName}" + Environment.NewLine +
                            $"  Instance assembly: {instance.GetType().Assembly.FullName}" + Environment.NewLine +
                            $"  Expected ScriptBehaviour from: {typeof(ScriptBehaviour).Assembly.FullName}" + Environment.NewLine +
                            $"  Instance's ScriptBehaviour from: {instance.GetType().BaseType?.Assembly.FullName ?? "unknown"}" + Environment.NewLine +
                            $"SOLUTION: Ensure EchoesBelow.dll references the same GrapeEngine.ScriptAPI.dll that is currently loaded", LogLevel.Error);
                return 0;
            }

            // Assign handle and store
            var handle = m_nextHandle++;
            m_instances[handle] = scriptBehaviour;

            Logging.Log($"Script instance created successfully. Handle: {handle}", LogLevel.Info);

            return handle;
        }
        catch (Exception ex)
        {
            Logging.Log($"Error creating script instance: {ex.Message}" + Environment.NewLine +
                        $"Stack trace: {ex.StackTrace}", LogLevel.Error);

            Marshal.FreeHGlobal(typeNamePtr); // nearly forgot to free the unmanaged string

            return 0;
        }
    }

    /// <summary>
    /// Destroy a script instance.
    /// Called from C++ when detaching a script from an entity.
    /// </summary>
    /// <param name="handle">The script instance handle.</param>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void DestroyScriptInstance(ulong handle)
    {
        try
        {
            if (m_instances.TryGetValue(handle, out var instance))
            {
                Logging.Log($"Destroying script instance. Handle: {handle}", LogLevel.Info);
                instance.OnDestroy();
                m_instances.Remove(handle);
            }
            else
            {
                Logging.Log($"Tried to destroy non-existent handle: {handle}", LogLevel.Warning);
            }
        }
        catch (Exception ex)
        {
            Logging.Log($"Error destroying script: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnStart on a script instance.
    /// Called from C++ during script initialization.
    /// </summary>
    /// <param name="handle">The script instance handle.</param>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void CallStart(ulong handle)
    {
        try
        {
            if (m_instances.TryGetValue(handle, out var instance))
            {
                instance.OnStart();
            }
            else
            {
                Logging.Log($"CallStart on invalid handle: {handle}", LogLevel.Warning);
            }
        }
        catch (Exception ex)
        {
            Logging.Log($"ERROR in OnStart: {ex.Message}" + Environment.NewLine +
                        $"Stack trace: {ex.StackTrace}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnUpdate on a script instance.
    /// Called from C++ every frame.
    /// </summary>
    /// <param name="handle">The script instance handle.</param>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void CallUpdate(ulong handle)
    {
        try
        {
            if (m_instances.TryGetValue(handle, out var instance))
            {
                instance.OnUpdate();
            }
        }
        catch (Exception ex)
        {
            Logging.Log($"ERROR in OnUpdate: {ex.Message}" + Environment.NewLine +
                        $"Stack trace: {ex.StackTrace}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnFixedUpdate on a script instance.
    /// Called from C++ at fixed time intervals for physics updates.
    /// </summary>
    /// <param name="handle">The script instance handle.</param>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void CallFixedUpdate(ulong handle)
    {
        try
        {
            if (m_instances.TryGetValue(handle, out var instance))
            {
                instance.OnFixedUpdate();
            }
        }
        catch (Exception ex)
        {
            Logging.Log($"ERROR in OnFixedUpdate: {ex.Message}" + Environment.NewLine +
                        $"Stack trace: {ex.StackTrace}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnLateUpdate on a script instance.
    /// Called from C++ every frame after all OnUpdate calls.
    /// </summary>
    /// <param name="handle">The script instance handle.</param>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void CallLateUpdate(ulong handle)
    {
        try
        {
            if (m_instances.TryGetValue(handle, out var instance))
            {
                instance.OnLateUpdate();
            }
        }
        catch (Exception ex)
        {
            Logging.Log($"ERROR in OnLateUpdate: {ex.Message}" + Environment.NewLine +
                        $"Stack trace: {ex.StackTrace}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnEnable on a script instance.
    /// </summary>
    /// <param name="handle">The script instance handle.</param>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void CallEnable(ulong handle)
    {
        try
        {
            if (m_instances.TryGetValue(handle, out var instance))
            {
                instance.OnEnable();
            }
        }
        catch (Exception ex)
        {
            Logging.Log($"ERROR in OnEnable: {ex.Message}" + Environment.NewLine +
                        $"Stack trace: {ex.StackTrace}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnDisable on a script instance.
    /// </summary>
    /// <param name="handle">The script instance handle.</param>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void CallDisable(ulong handle)
    {
        try
        {
            if (m_instances.TryGetValue(handle, out var instance))
            {
                instance.OnDisable();
            }
        }
        catch (Exception ex)
        {
            Logging.Log($"ERROR in OnDisable: {ex.Message}" + Environment.NewLine +
                        $"Stack trace: {ex.StackTrace}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Get the number of active script instances (for debugging).
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int GetInstanceCount()
    {
        return m_instances.Count;
    }

    /// <summary>
    /// Load an additional assembly into the AppDomain.
    /// This is needed to make custom script types discoverable.
    /// </summary>
    /// <param name="assemblyPathPtr">Pointer to C-string containing assembly path</param>
    /// <returns>1 if successful, 0 if failed</returns>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int LoadGameAssembly(IntPtr assemblyPathPtr)
    {
        try
        {
            var assemblyPath = Marshal.PtrToStringAnsi(assemblyPathPtr);
            if (string.IsNullOrWhiteSpace(assemblyPath))
            {
                Logging.Log("Null or empty assembly path", LogLevel.Error);
                return 0;
            }

            Logging.Log($"Loading game assembly: {assemblyPath}", LogLevel.Info);

            // Register assembly resolver to find ScriptAPI in the same directory
            if (!s_resolverRegistered)
            {
                var assemblyDir = Path.GetDirectoryName(assemblyPath);
                AppDomain.CurrentDomain.AssemblyResolve += (sender, args) =>
                {
                    Logging.Log($"Resolving assembly: {args.Name}", LogLevel.Info);
                    
                    // Parse the assembly name
                    var requestedName = new AssemblyName(args.Name);
                    
                    // CRITICAL: If the game assembly is asking for GrapeEngine.ScriptAPI,
                    // return the ALREADY LOADED ScriptAPI assembly to ensure type identity
                    if (requestedName.Name == "GrapeEngine.ScriptAPI")
                    {
                        var currentScriptAPI = typeof(ScriptBehaviour).Assembly;
                        Logging.Log($"Redirecting ScriptAPI reference to already loaded assembly: {currentScriptAPI.FullName}", LogLevel.Info);
                        return currentScriptAPI;
                    }
                    
                    // Try to find other dependencies in the game assembly directory
                    if (assemblyDir != null)
                    {
                        var dllPath = Path.Combine(assemblyDir, requestedName.Name + ".dll");
                        if (File.Exists(dllPath))
                        {
                            Logging.Log($"Loading dependency from: {dllPath}", LogLevel.Info);
                            return Assembly.LoadFrom(dllPath);
                        }
                    }
                    
                    return null;
                };
                s_resolverRegistered = true;
            }

            // The ScriptAPI assembly is already loaded into the current AppDomain by CoreCLR hosting.
            // Use Assembly.LoadFrom which will find dependencies in the same directory
            var fullPath = Path.GetFullPath(assemblyPath);
            var assembly = Assembly.LoadFrom(fullPath);
            
            if (assembly == null)
            {
                Logging.Log($"Failed to load assembly from {assemblyPath}", LogLevel.Error);
                return 0;
            }

            Logging.Log($"Successfully loaded assembly: {assembly.FullName}", LogLevel.Info);
            
            // Check which ScriptAPI this assembly references
            var scriptApiRef = assembly.GetReferencedAssemblies()
                .FirstOrDefault(a => a.Name == "GrapeEngine.ScriptAPI");
            
            if (scriptApiRef != null)
            {
                Logging.Log($"Game assembly references ScriptAPI version: {scriptApiRef.Version}", LogLevel.Info);
                Logging.Log($"Currently loaded ScriptAPI version: {typeof(ScriptBehaviour).Assembly.GetName().Version}", LogLevel.Info);
                
                if (scriptApiRef.Version != typeof(ScriptBehaviour).Assembly.GetName().Version)
                {
                    Logging.Log("WARNING: Version mismatch! Game assembly may not work correctly.", LogLevel.Warning);
                    Logging.Log("SOLUTION: Rebuild the game assembly (EchoesBelow.dll) to reference the current ScriptAPI", LogLevel.Warning);
                }
            }
            
            // Log all types found (for debugging)
            var types = assembly.GetTypes();
            Logging.Log($"Found {types.Length} types in assembly", LogLevel.Info);
            foreach (var type in types)
            {
                // Check base type with name comparison to avoid casting issues
                var baseType = type.BaseType;
                while (baseType != null)
                {
                    if (baseType.Name == "ScriptBehaviour")
                    {
                        Logging.Log($"\t- Script type found: {type.FullName}", LogLevel.Info);
                        Logging.Log($"\t  Base type assembly: {baseType.Assembly.FullName}", LogLevel.Debug);
                        break;
                    }
                    baseType = baseType.BaseType;
                }
            }

            return 1;
        }
        catch (Exception ex)
        {
            Logging.Log($"Error loading game assembly: {ex.Message}" + Environment.NewLine +
                        $"Stack trace: {ex.StackTrace}", LogLevel.Error);
            return 0;
        }
    }
}
