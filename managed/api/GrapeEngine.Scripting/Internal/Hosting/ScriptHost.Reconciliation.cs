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
        private static void ClearRequireForUpdateCache()
        {
            lock (s_stateLock)
            {
                _requireForUpdateCache.Clear();
            }
        }


        private static Dictionary<uint, ManagedComponentSchema> CaptureManagedComponentSchemas()
        {
            var schemas = new Dictionary<uint, ManagedComponentSchema>();

            // Capture a minimal schema fingerprint per managed component so reload can detect
            // removed components and binary-layout changes (size/signature drift).
            foreach (var type in ComponentDiscovery.TypeHashToType.Values)
            {
                try
                {
                    uint hash = ComponentTypeHelper.GetTypeHash(type);
                    int size = Marshal.SizeOf(type);
                    string signature = BuildComponentSignature(type);
                    string name = type.FullName ?? type.Name;
                    schemas[hash] = new ManagedComponentSchema(hash, size, signature, name);
                }
                catch (Exception ex) when (IsRecoverableInteropException(ex))
                {
                    Logging.LogInternal($"[ScriptHost] Failed to capture schema for {type.FullName}: {ex.Message}", LogLevel.Warning);
                }
            }

            return schemas;
        }


        private static unsafe Dictionary<uint, List<ManagedComponentSnapshot>> CaptureManagedComponentSnapshots(
            IntPtr worldPtr,
            IEnumerable<uint> componentHashes)
        {
            // Capture JSON snapshots keyed by component hash so changed schemas can be restored
            // onto surviving entities after assembly reload.
            var snapshots = new Dictionary<uint, List<ManagedComponentSnapshot>>();
            if (worldPtr == IntPtr.Zero)
                return snapshots;

            void* nativeWorldPtr = (void*)worldPtr;

            foreach (uint hash in componentHashes)
            {
                QueryIterator iterator = default;
                uint queryHash = hash;
                if (!QueryInteropAPI.CreateQuery(nativeWorldPtr, &queryHash, 1, null, 0, null, 0, &iterator))
                    continue;

                List<ManagedComponentSnapshot> perHash = new();
                ulong entityId = 0;
                while (QueryInteropAPI.QueryNext(&iterator, &entityId))
                {
                    // Serializer allocates UTF-8 memory on the native side; always free in finally.
                    nint jsonPtr = WorldAPI.SerializeComponentToJson(nativeWorldPtr, entityId, hash);
                    if (jsonPtr == IntPtr.Zero)
                        continue;

                    try
                    {
                        string json = Marshal.PtrToStringUTF8(jsonPtr) ?? "{}";
                        perHash.Add(new ManagedComponentSnapshot(entityId, json));
                    }
                    finally
                    {
                        WorldAPI.FreeSerializedString(jsonPtr);
                    }
                }

                if (perHash.Count > 0)
                {
                    snapshots[hash] = perHash;
                }
            }

            return snapshots;
        }


        private static string BuildComponentSignature(Type componentType)
        {
            // Keep declaration order stable by using metadata tokens, so signature comparison
            // does not flap when reflection enumeration order changes.
            var props = componentType.GetProperties(BindingFlags.Public | BindingFlags.Instance)
                .Where(p => p.GetIndexParameters().Length == 0)
                .OrderBy(p => p.MetadataToken)
                .Select(p => $"P:{p.Name}:{p.PropertyType.FullName}");

            // Ignore compiler-generated backing fields because property entries already represent
            // those members in the signature.
            var fields = componentType.GetFields(BindingFlags.Public | BindingFlags.Instance)
                .Where(f => !f.IsStatic && !f.Name.Contains("k__BackingField", StringComparison.Ordinal))
                .OrderBy(f => f.MetadataToken)
                .Select(f => $"F:{f.Name}:{f.FieldType.FullName}");

            return string.Join("|", props.Concat(fields));
        }


        private static void ReconcileManagedComponentsAfterReload()
        {
            if (!_hasPendingSchemaReconcile)
                return;

            // Reconcile in two phases:
            // 1) remove deleted/shape-changed component hashes globally
            // 2) restore JSON snapshots only for shape-changed hashes
            NativeRemoveComponentCallback? callback;
            lock (s_stateLock)
            {
                callback = _nativeRemoveComponent;
            }

            if (callback == null)
            {
                Logging.LogInternal("[ScriptHost] Cannot reconcile managed components: remove callback not registered", LogLevel.Warning);
                _reconcileWorldPtr = IntPtr.Zero;
                _preUnloadComponentSnapshots.Clear();
                _hasPendingSchemaReconcile = false;
                _preUnloadManagedSchemas.Clear();
                return;
            }

            var newSchemas = CaptureManagedComponentSchemas();
            var hashesToRemove = new HashSet<uint>();
            var hashesToRestore = new HashSet<uint>();

            foreach (var (hash, oldSchema) in _preUnloadManagedSchemas)
            {
                if (!newSchemas.TryGetValue(hash, out var newSchema))
                {
                    // Component removed from scripts: remove from all entities, do not restore.
                    hashesToRemove.Add(hash);
                    continue;
                }

                // Same hash but different layout: remove stale bytes, then restore via JSON.
                if (oldSchema.Size != newSchema.Size || !string.Equals(oldSchema.Signature, newSchema.Signature, StringComparison.Ordinal))
                {
                    hashesToRemove.Add(hash);
                    hashesToRestore.Add(hash);
                }
            }

            if (hashesToRemove.Count > 0)
            {
                foreach (uint hash in hashesToRemove)
                {
                    try
                    {
                        callback(hash);
                        Logging.LogInternal($"[ScriptHost] Removed outdated managed component hash 0x{hash:X8}", LogLevel.Info);
                    }
                    catch (Exception ex) when (IsRecoverableInteropException(ex))
                    {
                        Logging.LogInternal($"[ScriptHost] Failed removing managed component hash 0x{hash:X8}: {ex.Message}", LogLevel.Warning);
                    }
                }
            }

            // Restore changed components on entities that previously had them.
            // Deleted components are intentionally not restored.
            if (_reconcileWorldPtr != IntPtr.Zero && hashesToRestore.Count > 0)
            {
                unsafe
                {
                    void* nativeWorld = (void*)_reconcileWorldPtr;
                    foreach (uint hash in hashesToRestore)
                    {
                        if (!_preUnloadComponentSnapshots.TryGetValue(hash, out var entitySnapshots))
                            continue;
                        if (!newSchemas.TryGetValue(hash, out var schema))
                            continue;

                        foreach (var snap in entitySnapshots)
                        {
                            try
                            {
                                // Entity may have been destroyed during reload window.
                                if (!WorldAPI.IsEntityAlive(nativeWorld, snap.EntityId))
                                    continue;

                                if (!WorldAPI.HasComponent(nativeWorld, snap.EntityId, hash))
                                {
                                    // Add zeroed storage with the new size before JSON patching.
                                    WorldAPI.AddComponent(nativeWorld, snap.EntityId, hash, null, schema.Size);
                                }

                                WorldAPI.DeserializeComponentFromJson(nativeWorld, snap.EntityId, hash, snap.Json);
                            }
                            catch (Exception ex) when (IsRecoverableInteropException(ex))
                            {
                                Logging.LogInternal(
                                    $"[ScriptHost] Failed restoring component hash 0x{hash:X8} on entity {snap.EntityId}: {ex.Message}",
                                    LogLevel.Warning);
                            }
                        }
                    }
                }
            }

            _reconcileWorldPtr = IntPtr.Zero;
            _preUnloadComponentSnapshots.Clear();
            _preUnloadManagedSchemas.Clear();
            _hasPendingSchemaReconcile = false;
        }
}
