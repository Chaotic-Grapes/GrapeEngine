using System.Runtime.InteropServices;

namespace GrapeEngine.ScriptAPI.Unsafe
{
    /// <summary>
    /// Internal API - P/Invoke declarations for native engine functions.
    /// Do not call these directly - use ScriptBehaviour methods instead.
    /// </summary>
    internal static class EntityAPI
    {
        private const string NativeLib = "__Internal";

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe bool GetComponent(ulong entityId, uint typeHash, byte* outBuffer, int bufferSize);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void SetComponent(ulong entityId, uint typeHash, void* componentData, int dataSize);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool HasComponent(ulong entityId, uint typeHash);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RemoveComponent(ulong entityId, uint typeHash);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void DestroyEntity(ulong entityId);
    }
}
