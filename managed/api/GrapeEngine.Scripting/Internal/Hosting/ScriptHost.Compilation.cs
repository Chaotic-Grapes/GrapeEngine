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

        /// <summary>
        /// Compile all .cs files in a directory into an assembly using Roslyn.
        /// Called from C++ to compile scripts in-editor.
        /// </summary>
        [UnmanagedCallersOnly]
        public static int CompileScriptsInDirectory(IntPtr scriptsDirPtr, IntPtr outputAssemblyPathPtr)
        {
            try
            {
                var dir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
                var outPath = Marshal.PtrToStringUTF8(outputAssemblyPathPtr) ?? string.Empty;

                Logging.LogInternal($"[ScriptHost] CompileScriptsInDirectory: {dir} -> {outPath}", LogLevel.Info);

                int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
                return res;
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] CompileScriptsInDirectory error: {ex}", LogLevel.Error);
                return -1;
            }
        }


        /// <summary>
        /// Compile directory and return pointer to UTF8 diagnostics string (allocated with CoTaskMemAlloc).
        /// Caller must free the returned pointer using FreeStringFromManaged.
        /// Returns IntPtr.Zero on failure to allocate.
        /// </summary>
        [UnmanagedCallersOnly]
        public static IntPtr CompileDirectoryWithDiagnostics(IntPtr scriptsDirPtr, IntPtr outputAssemblyPathPtr)
        {
            try
            {
                var dir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
                var outPath = Marshal.PtrToStringUTF8(outputAssemblyPathPtr) ?? string.Empty;

                Logging.LogInternal($"[ScriptHost] CompileDirectoryWithDiagnostics: {dir} -> {outPath}", LogLevel.Info);

                int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);

                var diags = RoslynCompiler.GetLastDiagnostics() ?? string.Empty;
                // Encode as UTF8 and allocate unmanaged memory
                var bytes = System.Text.Encoding.UTF8.GetBytes(diags + '\0');
                IntPtr p = Marshal.AllocHGlobal(bytes.Length);
                Marshal.Copy(bytes, 0, p, bytes.Length);

                return p;
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] CompileDirectoryWithDiagnostics error: {ex}", LogLevel.Error);
                return IntPtr.Zero;
            }
        }


        [UnmanagedCallersOnly]
        public static void FreeStringFromManaged(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero)
                return;
            Marshal.FreeHGlobal(ptr);
        }


        /// <summary>
        /// Get the actual path to the last compiled assembly (may be versioned).
        /// Returns pointer to UTF8 string allocated with Marshal.AllocHGlobal.
        /// Returns IntPtr.Zero if no assembly has been compiled yet.
        /// Caller must free with FreeStringFromManaged.
        /// </summary>
        [UnmanagedCallersOnly]
        public static IntPtr GetLastCompiledAssemblyPath()
        {
            try
            {
                var path = RoslynCompiler.GetLastCompiledTempAssemblyPath();
                if (string.IsNullOrEmpty(path))
                    return IntPtr.Zero;

                var bytes = System.Text.Encoding.UTF8.GetBytes(path + '\0');
                IntPtr p = Marshal.AllocHGlobal(bytes.Length);
                Marshal.Copy(bytes, 0, p, bytes.Length);
                return p;
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] GetLastCompiledAssemblyPath fallback: {ex.Message}", LogLevel.Debug);
                return IntPtr.Zero;
            }
        }


        /// <summary>
        /// Compile scripts in directory (for initial editor compilation).
        /// Diagnostics-only version - returns compilation results without orchestrating reload.
        /// C++ controls all assembly lifecycle (unload, load, move).
        /// </summary>
        [UnmanagedCallersOnly]
        public static int CompileAndReload(IntPtr scriptsDirPtr, IntPtr outputAssemblyPathPtr)
        {
            try
            {
                var dir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
                var outPath = Marshal.PtrToStringUTF8(outputAssemblyPathPtr) ?? string.Empty;

                Logging.LogInternal($"[ScriptHost] CompileAndReload: {dir} -> {outPath}", LogLevel.Info);

                // C# only compiles - writes to temp location
                int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
                if (res != 0)
                {
                    Logging.LogInternal("[ScriptHost] Compilation failed", LogLevel.Error);
                    return -1;
                }

                // C++ will handle: unload -> move -> load -> re-discover systems
                // C# just reports success and provides temp path via GetLastCompiledTempAssemblyPath
                Logging.LogInternal($"[ScriptHost] Compilation succeeded - C++ will orchestrate assembly reload", LogLevel.Info);
                return 0;
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] CompileAndReload error: {ex}", LogLevel.Error);
                return -1;
            }
        }


        /// <summary>
        /// Generate a minimal .csproj file in the specified directory to help external IDEs
        /// (VS / Rider) open the script folder. Writes to <dir>/<projectName>.csproj.
        /// </summary>
        [UnmanagedCallersOnly]
        public static int GenerateCsProj(IntPtr outputDirPtr, IntPtr scriptsDirPtr, IntPtr projectNamePtr)
        {
            try
            {
                var outputDir = Marshal.PtrToStringUTF8(outputDirPtr) ?? string.Empty;
                var scriptsDir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
                var projectName = Marshal.PtrToStringUTF8(projectNamePtr) ?? string.Empty;

                if (string.IsNullOrWhiteSpace(projectName))
                {
                    Logging.LogInternal($"[ScriptHost] GenerateCsProj: invalid project name!", LogLevel.Error);
                    return -1;
                }

                if (string.IsNullOrWhiteSpace(outputDir) || !Directory.Exists(outputDir))
                {
                    Logging.LogInternal($"[ScriptHost] GenerateCsProj: invalid output dir {outputDir}", LogLevel.Warning);
                    return -1;
                }

                if (string.IsNullOrWhiteSpace(scriptsDir) || !Directory.Exists(scriptsDir))
                {
                    Logging.LogInternal($"[ScriptHost] GenerateCsProj: invalid scripts dir {scriptsDir}", LogLevel.Warning);
                    return -1;
                }

                string outPath = Path.Combine(outputDir, projectName + ".csproj");
                // Normalize the scripts directory path for the Include element
                string normalizedScriptsDir = Path.GetFullPath(scriptsDir);

                // .csproj template
                // Needs to:
                // - Target net9.0
                // - Include all .cs files in the scripts directory
                // - Reference GrapeEngine.Scripting for intellisense
                // - Reference GameScripts.dll for compiled types intellisense
                // - Exclude obj/ subdirectories
                string template = 
                $@"<Project Sdk=""Microsoft.NET.Sdk""> 
                    <PropertyGroup>
                        <TargetFramework>net9.0</TargetFramework>
                        <ImplicitUsings>enable</ImplicitUsings>
                        <Nullable>enable</Nullable>
                    </PropertyGroup>
                    <ItemGroup>
                        <Compile Include=""{normalizedScriptsDir}\**\*.cs"" />
                    </ItemGroup>
                    <ItemGroup>
                        <Reference Include=""GrapeEngine.Scripting"">
                            <HintPath>..\GrapeEngine.Scripting.dll</HintPath>
                        </Reference>
                    </ItemGroup>
                    <ItemGroup>
                        <Compile Remove=""{normalizedScriptsDir}\obj\**"" />
                    </ItemGroup>
                </Project>
                    ";

                File.WriteAllText(outPath, template);
                Logging.LogInternal($"[ScriptHost] Generated csproj: {outPath}", LogLevel.Info);
                return 0;
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] GenerateCsProj error: {ex.Message}", LogLevel.Error);
                return -1;
            }
        }


        private static IntPtr StringToHGlobalUtf8(string? s)
        {
            if (s == null)
                return IntPtr.Zero;

            // Encode as UTF8 and allocate unmanaged memory
            var bytes = System.Text.Encoding.UTF8.GetBytes(s + '\0');
            IntPtr p = Marshal.AllocHGlobal(bytes.Length); // +1 for null terminator
            Marshal.Copy(bytes, 0, p, bytes.Length); // copy including null terminator

            return p;
        }


        /// <summary>
        /// Map an HRESULT to the runtime's managed Exception representation and
        /// return a UTF8 pointer describing the exception type and message.
        /// Caller must free the returned pointer with FreeStringFromManaged.
        /// </summary>
        [UnmanagedCallersOnly]
        public static IntPtr GetManagedExceptionForHResult(int hr)
        {
            try
            {
                // Get the managed exception for the HRESULT
                Exception? ex = Marshal.GetExceptionForHR(hr);
                string s = ex == null 
                    ? "(no managed mapping)" 
                    : $"{ex.GetType().FullName}: {ex.Message}";

                // Allocate unmanaged UTF8 string
                return StringToHGlobalUtf8(s);
            }
            catch (Exception e) when (IsRecoverableInteropException(e))
            {
                return StringToHGlobalUtf8($"GetExceptionForHR threw: {e.GetType().FullName}: {e.Message}");
            }
        }


        /// <summary>
        /// Force a full garbage collection cycle from native code.
        /// This is exposed to native via UnmanagedCallersOnly so the host
        /// can explicitly request GC at critical points (e.g. after unload).
        /// </summary>
        [UnmanagedCallersOnly]
        public static void ForceGarbageCollection()
        {
            try
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
                GC.Collect();
                Logging.LogInternal("[ScriptHost] ForceGarbageCollection invoked from native", LogLevel.Debug);
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] ForceGarbageCollection error: {ex.Message}", LogLevel.Error);
            }
        }


        /// <summary>
        /// Return the number of diagnostics produced by the last Roslyn compilation.
        /// </summary>
        [UnmanagedCallersOnly]
        public static int GetLastDiagnosticsCount()
        {
            try
            {
                return RoslynCompiler.GetLastDiagnosticsCount();
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] GetLastDiagnosticsCount fallback: {ex.Message}", LogLevel.Debug);
                return 0;
            }
        }


        /// <summary>
        /// Return the diagnostic message at the specified index as an unmanaged UTF8 string.
        /// Caller must free the pointer with FreeStringFromManaged.
        /// </summary>
        [UnmanagedCallersOnly]
        public static IntPtr GetLastDiagnosticAt(int index)
        {
            try
            {
                string? s = RoslynCompiler.GetLastDiagnosticAt(index);
                return StringToHGlobalUtf8(s ?? string.Empty);
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] GetLastDiagnosticAt fallback: {ex.Message}", LogLevel.Debug);
                return IntPtr.Zero;
            }
        }


        /// <summary>
        /// Compile scripts in a directory into an assembly.
        /// Called from C++ to perform Roslyn compilation on a background thread.
        /// Returns 0 on success, -1 on failure.
        /// 
        /// C++ owns orchestration - unload/load/reload is handled by C++.
        /// This function only compiles. The compiled assembly is written to a temp location.
        /// C++ moves it to the final location after it unloads the old assembly.
        /// </summary>
        public static int CompileScriptsManaged(string dir, string outPath)
        {
            try
            {
                Logging.LogInternal($"[ScriptHost] CompileScriptsManaged: {dir} -> {outPath}", LogLevel.Info);
                
                int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
                if (res == 0)
                {
                    Logging.LogInternal($"[ScriptHost] Compilation successful", LogLevel.Info);
                }
                else
                {
                    Logging.LogInternal($"[ScriptHost] Compilation failed", LogLevel.Error);
                }
                return res;
            }
            catch (Exception ex) when (IsRecoverableInteropException(ex))
            {
                Logging.LogInternal($"[ScriptHost] CompileScriptsManaged error: {ex.Message}", LogLevel.Error);
                return -1;
            }
        }
}
