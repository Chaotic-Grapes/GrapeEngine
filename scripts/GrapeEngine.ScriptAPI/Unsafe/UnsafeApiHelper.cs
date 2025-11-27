/* Start Header *****************************************************************/
/*!
\file   UnsafeApiHelper.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   29th October 2025
\brief
Helper class for unsafe API interop. Internal use only. To be expanded later on.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Reflection;
using System.Runtime.InteropServices;

namespace GrapeEngine.ScriptAPI.Unsafe;

internal class UnsafeApiHelper
{
    // Placeholder library name
    // The actual resolution happens at DllImportResolver
    public const string NativeLib = "GrapeEngineNative";
    private static bool s_resolverInitialized = false;

    static UnsafeApiHelper()
    {
        Initialize();
    }

    /// <summary>
    /// Explicitly initialize the DLL import resolver.
    /// This is called automatically by the static constructor, but can also be called
    /// explicitly to ensure the resolver is set up before any P/Invoke calls.
    /// </summary>
    internal static void Initialize()
    {
        if (s_resolverInitialized)
            return;

        // Custom DLL import resolver for all assemblies in this namespace
        // This allows us to load native functions from the current process (main executable)
        // Works for both GrapeEngine.exe (editor) and any standalone game executable
        NativeLibrary.SetDllImportResolver(typeof(UnsafeApiHelper).Assembly, DllImportResolver);
        s_resolverInitialized = true;
    }

    private static IntPtr DllImportResolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        // If the library name matches our placeholder, return the main program handle
        if (libraryName == NativeLib)
        {
            try
            {
                // GetMainProgramHandle returns a handle to the currently executing program
                // This allows P/Invoke to find exported functions in the .exe file itself
                IntPtr handle = NativeLibrary.GetMainProgramHandle();
                
                if (handle == IntPtr.Zero)
                {
                    // Fallback: try to load the current executable explicitly
                    string exePath = System.Diagnostics.Process.GetCurrentProcess().MainModule?.FileName ?? "";
                    if (!string.IsNullOrEmpty(exePath))
                    {
                        handle = NativeLibrary.Load(exePath);
                    }
                }
                
                return handle;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[UnsafeApiHelper] Failed to resolve {libraryName}: {ex.Message}");
                return IntPtr.Zero;
            }
        }

        // For other libraries, use default resolution
        return IntPtr.Zero;
    }
}
