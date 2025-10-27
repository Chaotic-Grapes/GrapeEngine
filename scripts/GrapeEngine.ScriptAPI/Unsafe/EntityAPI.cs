using System.Runtime.InteropServices;

namespace GrapeEngine.ScriptAPI.Unsafe
{
    /// <summary>
    /// Internal API - P/Invoke declarations for native engine functions.
    /// Do not call these directly - use ScriptBehaviour methods instead.
    /// </summary>
    internal static class EntityAPI
    {
        private const string NativeLib = "GrapeEngine.exe";

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_GetComponent", CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe bool GetComponent(ulong entityId, uint typeHash, byte* outBuffer, int bufferSize);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_SetComponent", CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void SetComponent(ulong entityId, uint typeHash, void* componentData, int dataSize);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_HasComponent", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool HasComponent(ulong entityId, uint typeHash);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_RemoveComponent", CallingConvention = CallingConvention.Cdecl)]
        public static extern void RemoveComponent(ulong entityId, uint typeHash);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_DestroyEntity", CallingConvention = CallingConvention.Cdecl)]
        public static extern void DestroyEntity(ulong entityId);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_SetWorld", CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void SetWorld(void* world);
    }
}
