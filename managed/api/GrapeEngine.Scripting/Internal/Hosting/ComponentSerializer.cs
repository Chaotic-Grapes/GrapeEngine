/* Start Header *****************************************************************/
/*!
\file    ComponentSerializer.cs
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Component serialization helper for C# scripting components. Provides JSON
serialization functionality to the native engine via reverse P/Invoke. This
allows the engine to serialize C# components to JSON format for debugging, saving,
and other purposes, but most importantly for the editor.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

using System;
using System.Collections.Concurrent;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Internal.Hosting;

internal static class ComponentSerializer
{
    private static readonly ConcurrentDictionary<uint, Type> _types = new();
    private static readonly StringComparer _nameComparer = StringComparer.OrdinalIgnoreCase;
    private static readonly JsonSerializerOptions _jsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        IncludeFields = true
    };

    // Delegate used for reverse-P/Invoke from native (serialization)
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate IntPtr NativeSerializeDelegate(uint typeHash, IntPtr data, int size);

    // Delegate used for reverse-P/Invoke from native (deserialization)
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void NativeDeserializeDelegate(uint typeHash, IntPtr data, int size, IntPtr jsonStr);

    private static readonly NativeSerializeDelegate _nativeSerializeDelegate = ManagedSerializeCallback;
    private static readonly NativeDeserializeDelegate _nativeDeserializeDelegate = ManagedDeserializeCallback;
    private static readonly IntPtr _nativeSerializeFunctionPtr;
    private static readonly IntPtr _nativeDeserializeFunctionPtr;

    static ComponentSerializer()
    {
        _nativeSerializeFunctionPtr = Marshal.GetFunctionPointerForDelegate(_nativeSerializeDelegate);
        _nativeDeserializeFunctionPtr = Marshal.GetFunctionPointerForDelegate(_nativeDeserializeDelegate);

        // Register with native runtime so engine can call back into managed serializer/deserializer
        WorldAPI.RegisterSerializeCallback(_nativeSerializeFunctionPtr);
        WorldAPI.RegisterDeserializeCallback(_nativeDeserializeFunctionPtr);
    }

    public static void RegisterManagedType(uint hash, Type t)
    {
        _types[hash] = t;
    }

    internal static bool TryDeserializeFromJson(uint typeHash, IntPtr data, int size, string jsonString)
    {
        if (!_types.TryGetValue(typeHash, out var t))
            return false;

        return TryDeserializeToNative(t, data, size, jsonString);
    }

    /// <summary>
    /// Clear all registered types from the serializer cache.
    /// Called during assembly unload to break references to types from the loaded assembly.
    /// </summary>
    public static void ClearAllRegisteredTypes()
    {
        try
        {
            int count = _types.Count;
            _types.Clear();
            if (count > 0)
            {
                Logging.LogInternal($"[ComponentSerializer] Cleared {count} registered component types", LogLevel.Info);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ComponentSerializer] Error clearing registered types: {ex.Message}", LogLevel.Error);
        }
    }

    private static IntPtr ManagedSerializeCallback(uint typeHash, IntPtr data, int size)
    {
        try
        {
            if (!_types.TryGetValue(typeHash, out var t))
                return IntPtr.Zero;

            // Prefer blittable byte copy to avoid marshal layout remapping (bool/order issues).
            object? obj = null;
            try
            {
                obj = CreateManagedObjectFromNative(t, data, size);
            }
            catch
            {
                // Fallback to marshal path for unexpected edge cases.
                obj = Marshal.PtrToStructure(data, t);
            }

            if (obj == null)
                return IntPtr.Zero;

            // Serialize through neutral primitives/maps so System.Text.Json avoids
            // retaining type-specific dynamic accessors tied to collectible ALCs.
            object? jsonValue = ToJsonCompatibleValue(obj, t);
            var json = JsonSerializer.Serialize(jsonValue, _jsonOptions);

            // Return as CoTaskMem UTF8
            return StringToCoTaskMemUTF8(json);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ComponentSerializer] Serialize error: {ex.Message}", LogLevel.Error);
            return IntPtr.Zero;
        }
    }

    private static void ManagedDeserializeCallback(uint typeHash, IntPtr data, int size, IntPtr jsonStr)
    {
        try
        {
            if (!_types.TryGetValue(typeHash, out var t))
            {
                Logging.LogInternal($"[ComponentSerializer] Deserialize: Unknown type hash 0x{typeHash:X8}", LogLevel.Warning);
                return;
            }

            // Marshal JSON string from native
            string jsonString = Marshal.PtrToStringUTF8(jsonStr) ?? "{}";

            if (TryDeserializeToNative(t, data, size, jsonString))
            {
                return;
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ComponentSerializer] Deserialize error: {ex.Message}", LogLevel.Error);
        }
    }

    private static unsafe object? CreateManagedObjectFromNative(Type t, IntPtr data, int size)
    {
        if (size <= 0)
            return null;

        // Create an uninitialized boxed value-type and copy native bytes directly into it.
        // This preserves exact unmanaged layout for blittable script components.
        object boxed = RuntimeHelpers.GetUninitializedObject(t);
        GCHandle handle = default;
        try
        {
            handle = GCHandle.Alloc(boxed, GCHandleType.Pinned);
            IntPtr boxedPtr = handle.AddrOfPinnedObject();
            int managedSize = Marshal.SizeOf(t);
            if (size < managedSize)
            {
                Logging.LogInternal($"[ComponentSerializer] Serialize size mismatch for {t.Name}: native={size}, managed={managedSize}", LogLevel.Warning);
                return null;
            }

            Buffer.MemoryCopy((void*)data, (void*)boxedPtr, managedSize, managedSize);
            return boxed;
        }
        finally
        {
            if (handle.IsAllocated)
                handle.Free();
        }
    }

    private static unsafe bool TryDeserializeToNative(Type t, IntPtr data, int size, string jsonString)
    {
        object? obj;
        try
        {
            // Start from current native bytes so partial JSON updates keep existing values.
            obj = CreateManagedObjectFromNative(t, data, size);
        }
        catch
        {
            // Fallback: still allow full-object JSON writes when byte bootstrap fails.
            obj = RuntimeHelpers.GetUninitializedObject(t);
        }

        if (obj == null)
            return false;

        using JsonDocument document = JsonDocument.Parse(jsonString);
        // Apply JSON as a patch over the pre-seeded object, then copy final bytes to native.
        if (!ApplyJsonToObject(t, document.RootElement, obj))
        {
            Logging.LogInternal($"[ComponentSerializer] Failed to deserialize type {t.Name}", LogLevel.Warning);
            return false;
        }

        ResetRuntimeSentinelState(t, obj);

        GCHandle handle = default;
        try
        {
            // Copy updated managed bytes back to native storage used by ECS.
            handle = GCHandle.Alloc(obj, GCHandleType.Pinned);
            IntPtr boxedPtr = handle.AddrOfPinnedObject();
            int managedSize = Marshal.SizeOf(t);
            if (size < managedSize)
            {
                Logging.LogInternal($"[ComponentSerializer] Deserialize size mismatch for {t.Name}: native={size}, managed={managedSize}", LogLevel.Warning);
                return false;
            }

            Buffer.MemoryCopy((void*)boxedPtr, (void*)data, managedSize, managedSize);
            return true;
        }
        catch (Exception ex)
        {
            // Keep compatibility with prior behavior if pin/copy fails.
            Logging.LogInternal($"[ComponentSerializer] Byte-copy deserialize fallback for {t.Name}: {ex.Message}", LogLevel.Debug);
            try
            {
                Marshal.StructureToPtr(obj, data, false);
                return true;
            }
            catch
            {
                return false;
            }
        }
        finally
        {
            if (handle.IsAllocated)
                handle.Free();
        }
    }

    private static object? ToJsonCompatibleValue(object value, Type type)
    {
        // Nullable<T>
        Type? nullableUnderlying = Nullable.GetUnderlyingType(type);
        if (nullableUnderlying != null)
        {
            return ToJsonCompatibleValue(value, nullableUnderlying);
        }

        if (type.IsEnum)
            return Convert.ChangeType(value, Enum.GetUnderlyingType(type));

        if (type == typeof(bool) || type == typeof(byte) || type == typeof(sbyte) ||
            type == typeof(short) || type == typeof(ushort) || type == typeof(int) ||
            type == typeof(uint) || type == typeof(long) || type == typeof(ulong) ||
            type == typeof(float) || type == typeof(double) || type == typeof(decimal) ||
            type == typeof(char) || type == typeof(string))
        {
            return value;
        }

        if (type.IsValueType)
        {
            var map = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);

            foreach (var prop in type.GetProperties(BindingFlags.Public | BindingFlags.Instance))
            {
                if (!prop.CanRead || prop.GetIndexParameters().Length != 0)
                    continue;

                object? propVal = prop.GetValue(value);
                if (propVal == null)
                    continue;

                map[prop.Name] = ToJsonCompatibleValue(propVal, prop.PropertyType);
            }

            foreach (var field in type.GetFields(BindingFlags.Public | BindingFlags.Instance))
            {
                if (field.IsStatic)
                    continue;

                if (field.Name.Contains("k__BackingField", StringComparison.Ordinal))
                    continue;

                // Prefer property view when both exist.
                if (map.ContainsKey(field.Name))
                    continue;

                object? fieldVal = field.GetValue(value);
                if (fieldVal == null)
                    continue;

                map[field.Name] = ToJsonCompatibleValue(fieldVal, field.FieldType);
            }

            return map;
        }

        // Should never happen for unmanaged components, but keep fallback safe.
        return value.ToString();
    }

    private static bool ApplyJsonToObject(Type targetType, JsonElement element, object target)
    {
        if (element.ValueKind != JsonValueKind.Object)
            return false;

        // Build case-insensitive lookup maps once so each JSON property can be resolved
        // to either a writable property or a backing field.
        var properties = targetType.GetProperties(BindingFlags.Public | BindingFlags.Instance)
            .Where(p => p.GetIndexParameters().Length == 0)
            .ToDictionary(p => p.Name, p => p, StringComparer.OrdinalIgnoreCase);

        var fields = targetType.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)
            .Where(f => !f.IsStatic)
            .ToDictionary(f => f.Name, f => f, StringComparer.OrdinalIgnoreCase);

        foreach (var jsonProp in element.EnumerateObject())
        {
            if (properties.TryGetValue(jsonProp.Name, out var prop))
            {
                if (!TryConvertJsonToType(jsonProp.Value, prop.PropertyType, out object? converted))
                    continue;

                // init-only record structs cannot be set via setter after construction.
                // If a compiler backing field exists, write that field directly.
                string backingName = $"<{prop.Name}>k__BackingField";
                if (fields.TryGetValue(backingName, out var backingField))
                {
                    backingField.SetValue(target, converted);
                }
                else if (prop.CanWrite)
                {
                    prop.SetValue(target, converted);
                }

                continue;
            }

            if (fields.TryGetValue(jsonProp.Name, out var field))
            {
                if (field.Name.Contains("k__BackingField", StringComparison.Ordinal))
                    continue;

                if (!TryConvertJsonToType(jsonProp.Value, field.FieldType, out object? converted))
                    continue;

                field.SetValue(target, converted);
            }
        }

        return true;
    }

    private static void ResetRuntimeSentinelState(Type targetType, object target)
    {
        const string runtimeStartSentinel = "start";
        int resetCount = 0;

        foreach (var prop in targetType.GetProperties(BindingFlags.Public | BindingFlags.Instance))
        {
            if (prop.GetIndexParameters().Length != 0 || !prop.CanWrite || prop.PropertyType != typeof(bool))
                continue;

            if (!_nameComparer.Equals(prop.Name, runtimeStartSentinel))
                continue;

            if ((bool)(prop.GetValue(target) ?? false))
            {
                prop.SetValue(target, false);
                resetCount++;
            }
        }

        foreach (var field in targetType.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance))
        {
            if (field.IsStatic || field.IsInitOnly || field.FieldType != typeof(bool))
                continue;

            if (!_nameComparer.Equals(field.Name, runtimeStartSentinel))
                continue;

            if (field.Name.Contains("k__BackingField", StringComparison.Ordinal))
                continue;

            if ((bool)(field.GetValue(target) ?? false))
            {
                field.SetValue(target, false);
                resetCount++;
            }
        }

        if (resetCount > 0)
        {
            Logging.LogInternal(
                $"[ComponentSerializer] Reset runtime sentinel 'start' on {targetType.Name} ({resetCount} member(s))",
                LogLevel.Debug);
        }
    }

    private static bool TryConvertJsonToType(JsonElement element, Type targetType, out object? value)
    {
        value = null;

        Type? nullableUnderlying = Nullable.GetUnderlyingType(targetType);
        if (nullableUnderlying != null)
        {
            if (element.ValueKind == JsonValueKind.Null)
            {
                value = null;
                return true;
            }
            targetType = nullableUnderlying;
        }

        try
        {
            if (targetType == typeof(bool))
            {
                value = element.GetBoolean();
                return true;
            }
            if (targetType == typeof(byte)) { value = element.GetByte(); return true; }
            if (targetType == typeof(sbyte)) { value = element.GetSByte(); return true; }
            if (targetType == typeof(short)) { value = element.GetInt16(); return true; }
            if (targetType == typeof(ushort)) { value = element.GetUInt16(); return true; }
            if (targetType == typeof(int)) { value = element.GetInt32(); return true; }
            if (targetType == typeof(uint)) { value = element.GetUInt32(); return true; }
            if (targetType == typeof(long)) { value = element.GetInt64(); return true; }
            if (targetType == typeof(ulong)) { value = element.GetUInt64(); return true; }
            if (targetType == typeof(float)) { value = element.GetSingle(); return true; }
            if (targetType == typeof(double)) { value = element.GetDouble(); return true; }
            if (targetType == typeof(decimal)) { value = element.GetDecimal(); return true; }
            if (targetType == typeof(char))
            {
                string s = element.GetString() ?? string.Empty;
                if (s.Length == 1)
                {
                    value = s[0];
                    return true;
                }
                return false;
            }
            if (targetType == typeof(string))
            {
                value = element.GetString() ?? string.Empty;
                return true;
            }

            if (targetType.IsEnum)
            {
                if (element.ValueKind == JsonValueKind.String)
                {
                    // Support editor-friendly enum names in addition to numeric payloads.
                    string? s = element.GetString();
                    if (s != null && Enum.TryParse(targetType, s, ignoreCase: true, out object? enumObj))
                    {
                        value = enumObj;
                        return true;
                    }
                    return false;
                }

                var underlying = Enum.GetUnderlyingType(targetType);
                if (!TryConvertJsonToType(element, underlying, out object? enumRaw))
                    return false;

                value = Enum.ToObject(targetType, enumRaw!);
                return true;
            }

            if (targetType.IsValueType && element.ValueKind == JsonValueKind.Object)
            {
                // Nested unmanaged structs are recursively patched field-by-field.
                object boxed = RuntimeHelpers.GetUninitializedObject(targetType);
                if (!ApplyJsonToObject(targetType, element, boxed))
                    return false;

                value = boxed;
                return true;
            }
        }
        catch
        {
            return false;
        }

        return false;
    }

    private static IntPtr StringToCoTaskMemUTF8(string? str)
    {
        if (str == null)
            return IntPtr.Zero;

        var bytes = Encoding.UTF8.GetBytes(str);
        IntPtr mem = Marshal.AllocCoTaskMem(bytes.Length + 1);

        Marshal.Copy(bytes, 0, mem, bytes.Length);
        Marshal.WriteByte(mem, bytes.Length, 0);
        return mem;
    }
}


