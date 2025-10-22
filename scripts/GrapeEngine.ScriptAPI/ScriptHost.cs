using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

// Has hot reloading... theoretically?
// But it isn't tested yet.
namespace GrapeEngine.Scripting
{
    /// <summary>
    /// Entry point for C++ to interact with the managed scripting system.
    /// This class manages script instance lifecycle and dispatches calls.
    /// </summary>
    public static class ScriptHost
    {
        // Store all active script instances
        private static readonly Dictionary<ulong, ScriptBehaviour> s_instances = new();
        private static ulong s_nextHandle = 1;

        /// <summary>
        /// Create a new script instance.
        /// Called from C++ when attaching a script to an entity.
        /// </summary>
        /// <param name="typeNamePtr">Pointer to C-string containing fully qualified type name</param>
        /// <param name="entityId">The entity ID this script is attached to</param>
        /// <returns>Handle to the created instance, or 0 on failure</returns>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
        public static ulong CreateScriptInstance(IntPtr typeNamePtr, ulong entityId)
        {
            try
            {
                // Marshal type name from C++
                // My first time using Marshal class
                string? typeName = Marshal.PtrToStringAnsi(typeNamePtr);

                // I normally use string.IsNullOrWhiteSpace but I am going with IsNullOrEmpty just to be safe
                if (string.IsNullOrEmpty(typeName))
                {
                    Console.WriteLine("[ScriptHost] ERROR: Null or empty type name");
                    return 0;
                }

                Console.WriteLine($"[ScriptHost] Creating script instance: {typeName} for entity {entityId}");

                // Find the type
                Type? scriptType = Type.GetType(typeName);
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
                  Console.WriteLine($"[ScriptHost] ERROR: Type not found: {typeName}");
                  return 0;
                }

                // Verify it's a ScriptBehaviour
                if (!scriptType.IsSubclassOf(typeof(ScriptBehaviour)))
                {
                  Console.WriteLine($"[ScriptHost] ERROR: Type {typeName} does not inherit from ScriptBehaviour");
                  return 0;
                }

                // Create instance
                var instance = (ScriptBehaviour)Activator.CreateInstance(scriptType)!;
                instance.EntityId = entityId;

                // Assign handle and store
                ulong handle = s_nextHandle++;
                s_instances[handle] = instance;

                Console.WriteLine($"[ScriptHost] Script instance created successfully. Handle: {handle}");
                return handle;
            }
            catch (Exception ex)
            {
                // Honestly could just log ex.ToString() but whatever
                Console.WriteLine($"[ScriptHost] ERROR creating script instance: {ex.Message}");
                Console.WriteLine($"[ScriptHost] Stack trace: {ex.StackTrace}");
                return 0;
            }
        }

        /// <summary>
        /// Destroy a script instance.
        /// Called from C++ when detaching a script from an entity.
        /// </summary>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
        public static void DestroyScriptInstance(ulong handle)
        {
            try
            {
                if (s_instances.TryGetValue(handle, out var instance))
                {
                    Console.WriteLine($"[ScriptHost] Destroying script instance. Handle: {handle}");
                    instance.OnDestroy();
                    s_instances.Remove(handle);
                }
                else
                {
                    Console.WriteLine($"[ScriptHost] WARNING: Tried to destroy non-existent handle: {handle}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ScriptHost] ERROR destroying script: {ex.Message}");
            }
        }

        /// <summary>
        /// Call OnStart on a script instance.
        /// Called from C++ during script initialization.
        /// </summary>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
        public static void CallStart(ulong handle)
        {
            try
            {
                if (s_instances.TryGetValue(handle, out var instance))
                {
                    instance.OnStart();
                }
                else
                {
                    Console.WriteLine($"[ScriptHost] WARNING: CallStart on invalid handle: {handle}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ScriptHost] ERROR in OnStart: {ex.Message}");
                Console.WriteLine($"[ScriptHost] Stack trace: {ex.StackTrace}");
            }
        }

        /// <summary>
        /// Call OnUpdate on a script instance.
        /// Called from C++ every frame.
        /// </summary>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
        public static void CallUpdate(ulong handle, float deltaTime)
        {
            try
            {
                if (s_instances.TryGetValue(handle, out var instance))
                {
                    instance.OnUpdate(deltaTime);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ScriptHost] ERROR in OnUpdate: {ex.Message}");
            }
        }

        /// <summary>
        /// Get the number of active script instances (for debugging).
        /// </summary>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
        public static int GetInstanceCount()
        {
            return s_instances.Count;
        }
    }
}
