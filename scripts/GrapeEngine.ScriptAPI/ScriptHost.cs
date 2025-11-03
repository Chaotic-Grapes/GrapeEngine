using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

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
                Logging.Log($"ERROR: Type {typeName} does not inherit from ScriptBehaviour", LogLevel.Error);
                Logging.Log($"  Type's base: {scriptType.BaseType?.FullName ?? "none"}", LogLevel.Error);
                return 0;
            }

            // Create instance
            var instance = (ScriptBehaviour)Activator.CreateInstance(scriptType)!;
            instance.EntityId = entityId;

            // Assign handle and store
            var handle = m_nextHandle++;
            m_instances[handle] = instance;

            Logging.Log($"Script instance created successfully. Handle: {handle}", LogLevel.Info);

            return handle;
        }
        catch (Exception ex)
        {
            // Honestly could just log ex.ToString() but whatever
            Logging.Log($"Error creating script instance: {ex.Message}", LogLevel.Error);
            Logging.Log($"Stack trace: {ex.StackTrace}", LogLevel.Error);

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
            Logging.Log($"ERROR in OnStart: {ex.Message}", LogLevel.Error);
            Logging.Log($"Stack trace: {ex.StackTrace}", LogLevel.Error);
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
            Logging.Log($"ERROR in OnUpdate: {ex.Message}", LogLevel.Error);
            Logging.Log($"Stack trace: {ex.StackTrace}", LogLevel.Error);
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
            Logging.Log($"ERROR in OnFixedUpdate: {ex.Message}", LogLevel.Error);
            Logging.Log($"Stack trace: {ex.StackTrace}", LogLevel.Error);
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
            Logging.Log($"ERROR in OnLateUpdate: {ex.Message}", LogLevel.Error);
            Logging.Log($"Stack trace: {ex.StackTrace}", LogLevel.Error);
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
            Logging.Log($"ERROR in OnEnable: {ex.Message}", LogLevel.Error);
            Logging.Log($"Stack trace: {ex.StackTrace}", LogLevel.Error);
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
            Logging.Log($"ERROR in OnDisable: {ex.Message}", LogLevel.Error);
            Logging.Log($"Stack trace: {ex.StackTrace}", LogLevel.Error);
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

            // Load the assembly using AssemblyLoadContext to avoid type identity issues
            // First try to load by name if it's already in the same directory
            var assemblyName = AssemblyName.GetAssemblyName(assemblyPath);
            Assembly? assembly = null;
            
            try
            {
                // Try loading by name first (better for resolving dependencies)
                assembly = Assembly.Load(assemblyName);
            }
            catch
            {
                // If that fails, use LoadFrom
                assembly = Assembly.LoadFrom(assemblyPath);
            }
            
            if (assembly == null)
            {
                Logging.Log($"Failed to load assembly from {assemblyPath}", LogLevel.Error);
                return 0;
            }

            Logging.Log($"Successfully loaded assembly: {assembly.FullName}", LogLevel.Info);
            
            // Log all types found (for debugging)
            var types = assembly.GetTypes();
            Logging.Log($"Found {types.Length} types in assembly", LogLevel.Info);
            foreach (var type in types)
            {
                if (type.IsSubclassOf(typeof(ScriptBehaviour)))
                {
                    Logging.Log($"\t- Script type found: {type.FullName}", LogLevel.Info);
                }
            }

            return 1;
        }
        catch (Exception ex)
        {
            Logging.Log($"Error loading game assembly: {ex.Message}", LogLevel.Error);
            Logging.Log($"Stack trace: {ex.StackTrace}", LogLevel.Error);
            return 0;
        }
    }
}
