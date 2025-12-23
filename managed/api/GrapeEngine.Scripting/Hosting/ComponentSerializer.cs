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
using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Hosting;

internal static class ComponentSerializer
{
    private static readonly ConcurrentDictionary<uint, Type> _types = new();

    // Delegate used for reverse-P/Invoke from native
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate IntPtr NativeSerializeDelegate(uint typeHash, IntPtr data, int size);

    private static readonly NativeSerializeDelegate _nativeDelegate = ManagedSerializeCallback;
    private static readonly IntPtr _nativeFunctionPtr;

    static ComponentSerializer()
    {
        _nativeFunctionPtr = Marshal.GetFunctionPointerForDelegate(_nativeDelegate);

        // Register with native runtime so engine can call back into managed serializer
        WorldAPI.RegisterSerializeCallback(_nativeFunctionPtr);
    }

    public static void RegisterManagedType(uint hash, Type t)
    {
        _types[hash] = t;
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
            Console.WriteLine($"[ComponentSerializer] Serialize error: {ex.Message}");
            return IntPtr.Zero;
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
