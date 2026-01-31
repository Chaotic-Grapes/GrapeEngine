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
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Internal.Hosting;

internal static class ComponentSerializer
{
    private static readonly ConcurrentDictionary<uint, Type> _types = new();

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

            // Marshal native bytes into managed object
            var obj = Marshal.PtrToStructure(data, t)!;

            // Serialize to JSON
            var json = JsonSerializer.Serialize(obj, t);

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

            // Deserialize from JSON
            var options = new JsonSerializerOptions 
            { 
                PropertyNameCaseInsensitive = true,
                IncludeFields = true
            };
            var obj = JsonSerializer.Deserialize(jsonString, t, options);

            if (obj == null)
            {
                Logging.LogInternal($"[ComponentSerializer] Failed to deserialize type {t.Name}", LogLevel.Warning);
                return;
            }

            // Copy deserialized object back to native memory
            Marshal.StructureToPtr(obj, data, false);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ComponentSerializer] Deserialize error: {ex.Message}", LogLevel.Error);
        }
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


