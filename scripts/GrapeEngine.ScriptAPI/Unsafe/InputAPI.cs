using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Unsafe
{
    /// <summary>
    /// Low-level P/Invoke declarations for the Input API.
    /// These map directly to the C++ Input service functions.
    /// </summary>
    internal static class InputAPI
    {
        private const string NativeLib = "GrapeEngine.exe";

        // Keyboard input
        [DllImport(NativeLib, EntryPoint = "ScriptAPI_IsKeyPressed", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool IsKeyPressed(int key);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_IsKeyDown", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool IsKeyDown(int key);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_IsKeyUp", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool IsKeyUp(int key);

        // Mouse input
        [DllImport(NativeLib, EntryPoint = "ScriptAPI_IsMousePressed", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool IsMousePressed(int button);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_GetMouseX", CallingConvention = CallingConvention.Cdecl)]
        public static extern double GetMouseX();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_GetMouseY", CallingConvention = CallingConvention.Cdecl)]
        public static extern double GetMouseY();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_GetScrollX", CallingConvention = CallingConvention.Cdecl)]
        public static extern double GetScrollX();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_GetScrollY", CallingConvention = CallingConvention.Cdecl)]
        public static extern double GetScrollY();

        // Window dimensions
        [DllImport(NativeLib, EntryPoint = "ScriptAPI_GetWindowWidth", CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetWindowWidth();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_GetWindowHeight", CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetWindowHeight();
    }
}
