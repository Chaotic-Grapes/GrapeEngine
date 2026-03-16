using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Core.Dependencies;
using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Reflection;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Hosting;

public static partial class ScriptHost
{
    private static bool IsRecoverableSystemCallbackException(Exception ex)
    {
        return ex is not OutOfMemoryException and not StackOverflowException;
    }

    private static string GetSystemLabel(ulong handle)
    {
        Type? systemType = SystemDiscovery.GetSystemType(handle);
        if (systemType != null)
        {
            return systemType.FullName ?? systemType.Name;
        }

        return "unknown system";
    }

        /// <summary>
        /// Called from C++ when creating ScriptSystemWrapper.
        /// </summary>
        [UnmanagedCallersOnly]
        public static ulong CreateSystemInstance(IntPtr typeNamePtr)
        {
            try
            {
                var typeName = Marshal.PtrToStringUTF8(typeNamePtr) ?? string.Empty;

                // Find the type in loaded assemblies
                Type? systemType = null;
                foreach (var assembly in AssemblyManager.GetAllLoadedAssemblies())
                {
                    systemType = assembly.GetType(typeName);
                    if (systemType != null)
                        break;
                }

                if (systemType == null)
                {
                    Logging.LogInternal($"[ScriptHost] System type not found: {typeName}", LogLevel.Warning);
                    return 0;
                }

                // Create instance and get handle
                ulong handle = SystemDiscovery.CreateSystemInstanceFromType(systemType);
                if (handle == 0)
                {
                    Logging.LogInternal($"[ScriptHost] Failed to create instance of: {typeName}", LogLevel.Warning);
                    return 0;
                }

                Logging.LogInternal($"[ScriptHost] Created system instance: {typeName} (handle: {handle})", LogLevel.Info);
                return handle;
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] Error creating system instance: {ex.Message}", LogLevel.Error);
                return 0;
            }
        }


        /// <summary>
        /// Destroy a system instance.
        /// </summary>
        [UnmanagedCallersOnly]
        public static void DestroySystemInstance(ulong handle)
        {
            // Note: Instances remain in SystemDiscovery's dictionary until next reload
            // This is called to notify C++ that the managed system is being destroyed
        }


        /// <summary>
        /// Get metadata about a system (name, group, run mode).
        /// </summary>
        [UnmanagedCallersOnly]
        public static void GetSystemMetadata(ulong handle, IntPtr outNameBuffer, IntPtr outGroupPtr, IntPtr outRunModePtr, IntPtr outOrderPtr)
        {
            try
            {
                Type? systemType = SystemDiscovery.GetSystemType(handle);
                if (systemType == null)
                {
                    Logging.LogInternal($"[ScriptHost] System handle not found: {handle}", LogLevel.Warning);
                    return;
                }
                
                // Get instance if available (for ISystemMetadata interface)
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                
                // Get name
                string name = systemType.FullName ?? systemType.Name;
                byte[] nameBytes = System.Text.Encoding.UTF8.GetBytes(name);
                if (outNameBuffer != IntPtr.Zero)
                {
                    const int maxLen = 256;
                    int copyLen = System.Math.Min(nameBytes.Length, maxLen - 1);
                    if (copyLen > 0)
                    {
                        Marshal.Copy(nameBytes, 0, outNameBuffer, copyLen);
                    }
                    Marshal.WriteByte(outNameBuffer, copyLen, 0);
                }
                
                // Get group from ISystemMetadata interface first, then [SystemGroup] attribute
                if (outGroupPtr != IntPtr.Zero)
                {
                    Marshal.WriteInt32(outGroupPtr, (int)SystemMetadataExtractor.GetSystemGroup(systemType, instance));
                }
                
                // Same as above, but for SystemRunMode
                if (outRunModePtr != IntPtr.Zero)
                {
                    Marshal.WriteInt32(outRunModePtr, (int)SystemMetadataExtractor.GetSystemRunMode(systemType, instance));
                }

                if (outOrderPtr != IntPtr.Zero)
                {
                    Marshal.WriteInt32(outOrderPtr, SystemMetadataExtractor.GetSystemOrder(systemType));
                }
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] Error getting metadata: {ex.Message}", LogLevel.Error);
            }
        }


        /// <summary>
        /// Extract component access information from a scripted system for C++ dependency resolution.
        /// 
        /// Returns two arrays of component type hashes:
        /// 1. Read-only components (safe for parallel access)
        /// 2. Write-access components (exclusive access required)
        /// 
        /// Called from C++ during system registration to populate ComponentAccessBuilder metadata.
        /// </summary>
        [UnmanagedCallersOnly(EntryPoint = "GetSystemComponentAccesses")]
        public static int GetSystemComponentAccesses(ulong handle, IntPtr outReadHashesPtr, IntPtr outWriteHashesPtr, int maxSize)
        {
            try
            {
                Type? systemType = SystemDiscovery.GetSystemType(handle);
                if (systemType == null)
                {
                    Logging.LogInternal($"[ScriptHost] System handle not found: {handle}", LogLevel.Warning);
                    return 0;
                }

                // Extract component accesses from C# attributes using ComponentAccessBridge
                var accesses = ComponentAccessBridge.ExtractComponentAccesses(systemType);
                
                // Separate into read and write accesses
                int readCount = 0;
                int writeCount = 0;
                var readHashes = new List<uint>();
                var writeHashes = new List<uint>();

                foreach (var (hash, mode) in accesses)
                {
                    if (mode == ComponentAccessMode.Read)
                    {
                        if (readCount < maxSize / 2)
                        {
                            readHashes.Add(hash);
                            readCount++;
                        }
                    }
                    else // Write or ReadWrite
                    {
                        if (writeCount < maxSize / 2)
                        {
                            writeHashes.Add(hash);
                            writeCount++;
                        }
                    }
                }
                
                // Copy arrays to unmanaged memory
                if (outReadHashesPtr != IntPtr.Zero && readCount > 0)
                {
                    // Marshal.Copy does not have a uint[] overload, use int[] with identical bit-patterns
                    int[] tmp = new int[readCount];
                    for (int i = 0; i < readCount; ++i)
                        tmp[i] = unchecked((int)readHashes[i]);

                    Marshal.Copy(tmp, 0, outReadHashesPtr, readCount);
                }
                if (outWriteHashesPtr != IntPtr.Zero && writeCount > 0)
                {
                    int[] tmp = new int[writeCount];
                    for (var i = 0; i < writeCount; ++i)
                        tmp[i] = unchecked((int)writeHashes[i]);

                    Marshal.Copy(tmp, 0, outWriteHashesPtr, writeCount);
                }

                // Return total count (read count in lower 16 bits, write count in upper 16 bits)
                return (readCount) | (writeCount << 16);
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] Error getting component accesses: {ex.Message}", LogLevel.Error);
                return 0;
            }
        }


        /// <summary>
        /// Hash a component type name using FNV-1a algorithm in C++.
        /// Delegates to C++ to ensure hash consistency across language boundary.
        /// </summary>
        [UnmanagedCallersOnly]
        public static uint HashComponentTypeName(IntPtr typeNamePtr)
        {
            try
            {
                // For now, compute here but ideally this would call C++
                var typeName = Marshal.PtrToStringUTF8(typeNamePtr) ?? string.Empty;
                return ComponentAccessBridge.Fnv1aHashPublic(typeName);
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] Error hashing component type: {ex.Message}", LogLevel.Error);
                return 0;
            }
        }


        /// <summary>
        /// Resolve a system's execution group using the priority system in C++.
        /// 
        /// C# reflection extracts the [SystemGroup] attribute,
        /// C++ validates and applies priority resolution.
        /// 
        /// This keeps metadata priority logic centralized in C++ while C# handles reflection.
        /// </summary>
        [UnmanagedCallersOnly]
        public static int ResolveSystemGroup(int attributeGroup)
        {
            try
            {
                // For now, return directly (C# already applied priority correctly)
                // In future: Could validate/transform the group with C++ logic
                return attributeGroup;
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] Error resolving system group: {ex.Message}", LogLevel.Error);
                return 0; // Default to Update
            }
        }


        /// <summary>
        /// Call OnCreate on a scripted system.
        /// </summary>
        [UnmanagedCallersOnly]
        public static void CallSystemOnCreate(ulong handle, IntPtr worldPtr)
        {
            try
            {
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                if (instance == null)
                {
                    Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                    return;
                }
                
                if (instance is ISystem system)
                {
                    // Use cached World wrapper to avoid unnecessary allocations
                    World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                    system.OnCreate(managedWorld);
                }
            }
            catch (Exception ex) when (IsRecoverableSystemCallbackException(ex))
            {
                Logging.LogInternal($"[ScriptHost] OnCreate failed in {GetSystemLabel(handle)}: {ex}", LogLevel.Error);
            }
        }



        /// <summary>
        /// Call OnUpdate on a scripted system.
        /// </summary>
        [UnmanagedCallersOnly]
        public static void CallSystemOnUpdate(ulong handle, IntPtr worldPtr)
        {
            try
            {
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                if (instance == null)
                {
                    Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                    return;
                }
                
                if (instance is ISystem system)
                {
                    // Use cached World wrapper to avoid allocating a new object every frame
                    World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                    system.OnUpdate(managedWorld);
                }
            }
            catch (Exception ex) when (IsRecoverableSystemCallbackException(ex))
            {
                Logging.LogInternal($"[ScriptHost] OnUpdate failed in {GetSystemLabel(handle)}: {ex}", LogLevel.Error);
            }
        }

        private static uint[] GetRequireForUpdateHashes(ulong handle)
        {
            // Check cache first to avoid reflection on every ShouldRun call
            lock (s_stateLock)
            {
                if (_requireForUpdateCache.TryGetValue(handle, out uint[]? cachedHashes))
                {
                    return cachedHashes;
                }
            }

            // If not in cache, extract from system type and cache it
            Type? systemType = SystemDiscovery.GetSystemType(handle);
            if (systemType == null)
            {
                uint[] emptyHashes = [];
                lock (s_stateLock)
                {
                    _requireForUpdateCache[handle] = emptyHashes;
                }
                return emptyHashes;
            }

            uint[] hashes = ComponentAccessBridge.ExtractRequireForUpdateComponentHashes(systemType).ToArray();
            lock (s_stateLock)
            {
                _requireForUpdateCache[handle] = hashes;
            }
            return hashes;
        }


        private static unsafe bool MatchesRequireForUpdate(World world, uint[] requiredHashes)
        {
            if (requiredHashes.Length == 0)
            {
                return true;
            }

            // A system should run only if at least one entity satisfies RequireForUpdate.
            fixed (uint* reqPtr = requiredHashes)
            {
                QueryIterator iterator = default;
                QueryInteropAPI.CreateQuery(world.NativePtr, reqPtr, requiredHashes.Length, null, 0, null, 0, &iterator);
                ulong entityId = 0;
                return QueryInteropAPI.QueryNext(&iterator, &entityId);
            }
        }


        /// <summary>
        /// Call ShouldRun on a scripted system.
        /// </summary>
        [UnmanagedCallersOnly]
        public static int CallSystemShouldRun(ulong handle, IntPtr worldPtr)
        {
            try
            {
                // Get the system instance for this handle
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                if (instance == null)
                {
                    Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                    return 0;
                }

                // If the system implements ISystem, call ShouldRun and return the result
                if (instance is ISystem system)
                {
                    World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                    uint[] requiredHashes = GetRequireForUpdateHashes(handle);
                    if (!MatchesRequireForUpdate(managedWorld, requiredHashes))
                    {
                        return 0;
                    }
                    return system.ShouldRun(managedWorld) ? 1 : 0;
                }
            }
            catch (Exception ex) when (IsRecoverableSystemCallbackException(ex))
            {
                Logging.LogInternal($"[ScriptHost] ShouldRun failed in {GetSystemLabel(handle)}: {ex}", LogLevel.Error);
            }

            return 0;
        }



        /// <summary>
        /// Call OnDestroy on a scripted system.
        /// </summary>
        [UnmanagedCallersOnly]
        public static void CallSystemOnDestroy(ulong handle, IntPtr worldPtr)
        {
            try
            {
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                if (instance == null)
                {
                    Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                    return;
                }
                
                if (instance is ISystem system)
                {
                    // Use cached World wrapper to avoid unnecessary allocations
                    World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                    system.OnDestroy(managedWorld);
                }
            }
            catch (Exception ex) when (IsRecoverableSystemCallbackException(ex))
            {
                Logging.LogInternal($"[ScriptHost] OnDestroy failed in {GetSystemLabel(handle)}: {ex}", LogLevel.Error);
            }
        }


        /// <summary>
        /// Call OnSceneStart on a scripted system.
        /// </summary>
        [UnmanagedCallersOnly]
        public static void CallSystemOnSceneStart(ulong handle)
        {
            try
            {
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                if (instance == null)
                {
                    Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                    return;
                }

                if (instance is ISystem system)
                {
                    system.OnSceneStart();
                }
            }
            catch (Exception ex) when (IsRecoverableSystemCallbackException(ex))
            {
                Logging.LogInternal($"[ScriptHost] OnSceneStart failed in {GetSystemLabel(handle)}: {ex}", LogLevel.Error);
            }
        }


        /// <summary>
        /// Call OnSceneStop on a scripted system.
        /// </summary>
        [UnmanagedCallersOnly]
        public static void CallSystemOnSceneStop(ulong handle)
        {
            try
            {
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                if (instance == null)
                {
                    Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                    return;
                }

                if (instance is ISystem system)
                {
                    system.OnSceneStop();
                }
            }
            catch (Exception ex) when (IsRecoverableSystemCallbackException(ex))
            {
                Logging.LogInternal($"[ScriptHost] OnSceneStop failed in {GetSystemLabel(handle)}: {ex}", LogLevel.Error);
            }
        }


        /// <summary>
        /// Call OnStartRunning on a scripted system.
        /// </summary>
        [UnmanagedCallersOnly]
        public static void CallSystemOnStartRunning(ulong handle, IntPtr worldPtr)
        {
            try
            {
                // Get the system instance for this handle and call OnStartRunning if it implements ISystem
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                if (instance == null)
                {
                    Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                    return;
                }

                if (instance is ISystem system)
                {
                    World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                    system.OnStartRunning(managedWorld);
                }
            }
            catch (Exception ex) when (IsRecoverableSystemCallbackException(ex))
            {
                Logging.LogInternal($"[ScriptHost] OnStartRunning failed in {GetSystemLabel(handle)}: {ex}", LogLevel.Error);
            }
        }


        /// <summary>
        /// Call OnStopRunning on a scripted system.
        /// </summary>
        [UnmanagedCallersOnly]
        public static void CallSystemOnStopRunning(ulong handle, IntPtr worldPtr)
        {
            try
            {
                // Get the system instance for this handle and call OnStopRunning if it implements ISystem
                object? instance = SystemDiscovery.GetSystemInstance(handle);
                if (instance == null)
                {
                    Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                    return;
                }

                if (instance is ISystem system)
                {
                    World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                    system.OnStopRunning(managedWorld);
                }
            }
            catch (Exception ex) when (IsRecoverableSystemCallbackException(ex))
            {
                Logging.LogInternal($"[ScriptHost] OnStopRunning failed in {GetSystemLabel(handle)}: {ex}", LogLevel.Error);
            }
        }
}
