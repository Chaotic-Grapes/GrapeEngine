using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

// Has hot reloading... theoretically?
// But it isn't tested yet.

// ************** !!!!!!!! IMPORTANT !!!!!!!! ************** //
//                                                           //
// Please consult me before making ANY changes to this file. //
// Unless it's just logging changes or documentation updates //
//                                                           //
// ************** !!!!!!!! IMPORTANT !!!!!!!! ************** //

namespace GrapeEngine.Scripting
{
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
        [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])] // CallConvCdecl is in System.Runtime.CompilerServices
        public static ulong CreateScriptInstance(IntPtr typeNamePtr, ulong entityId)
        {
            try
            {
                // Marshal type name from C++
                var typeName = Marshal.PtrToStringAnsi(typeNamePtr);

                // I normally use string.IsNullOrWhiteSpace but I am going with IsNullOrEmpty just to be safe
                if (string.IsNullOrEmpty(typeName))
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

                // Verify it's a ScriptBehaviour
                if (!scriptType.IsSubclassOf(typeof(ScriptBehaviour)))
                {
                    Logging.Log($"ERROR: Type {typeName} does not inherit from ScriptBehaviour", LogLevel.Error);
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
                    Logging.Log($"WARNING: Tried to destroy non-existent handle: {handle}", LogLevel.Warning);
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
                    Logging.Log($"WARNING: CallStart on invalid handle: {handle}", LogLevel.Warning);
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
    }
}
