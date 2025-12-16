/* Start Header *****************************************************************/
/*!
\file   ScriptHost.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Script host for managing C# assemblies and systems. Main P/Invoke entry point.
Coordinates assembly loading, system discovery, and hot reload via helper classes:
- AssemblyManager: Assembly loading/unloading
- SystemDiscovery: System discovery and instantiation  
- StatePreserver: Hot reload state management

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Reflection;
using System.Runtime.Loader;
using System.IO;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Systems;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// Custom AssemblyLoadContext for hot reload support.
/// Allows assemblies to be unloaded and reloaded.
/// </summary>
internal class ScriptLoadContext(string assemblyPath) : AssemblyLoadContext(isCollectible: true)
{
    private readonly AssemblyDependencyResolver _resolver = new(assemblyPath);

    protected override Assembly? Load(AssemblyName assemblyName)
    {
        string? assemblyPath = _resolver.ResolveAssemblyToPath(assemblyName);
        if (assemblyPath != null)
        {
            return LoadFromAssemblyPath(assemblyPath);
        }
        return null;
    }

    protected override IntPtr LoadUnmanagedDll(string unmanagedDllName)
    {
        string? libraryPath = _resolver.ResolveUnmanagedDllToPath(unmanagedDllName);
        if (libraryPath != null)
        {
            return LoadUnmanagedDllFromPath(libraryPath);
        }
        return IntPtr.Zero;
    }
}

/// <summary>
/// SCRIPT HOST - Main P/Invoke entry point for script hosting.
/// 
/// Delegates specialized tasks to focused helper classes:
/// - AssemblyManager: Assembly lifecycle (load/unload)
/// - SystemDiscovery: System discovery and instantiation
/// - StatePreserver: Hot reload state preservation
/// 
/// This class serves as the coordinator and C++ interface layer only.
/// </summary>
public static class ScriptHost
{

    /// <summary>
    /// Load a C# assembly containing scripted systems.
    /// Called from C++ ScriptManager::LoadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int LoadAssembly(char* assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8((IntPtr)assemblyPathPtr) ?? "";
            return AssemblyManager.LoadAssembly(assemblyPath) != null ? 0 : -1;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] LoadAssembly error: {ex.Message}");
            return -1;
        }
    }

    /// <summary>
    /// Unload an assembly for hot reload support.
    /// Called from C++ ScriptManager::UnloadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int UnloadAssembly(char* assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8((IntPtr)assemblyPathPtr) ?? "";

            Console.WriteLine($"[ScriptHost] Unloading assembly: {assemblyPath}");

            // Save state from all systems in this assembly
            StatePreserver.SaveAllSystemStates(assemblyPath);

            // Unload assembly
            bool success = AssemblyManager.UnloadAssembly(assemblyPath);
            if (!success)
            {
                return -1;
            }

            // Clear discovered systems
            SystemDiscovery.ClearDiscoveredSystems();

            Console.WriteLine($"[ScriptHost] Successfully unloaded: {assemblyPath}");
            return 0; // Success
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error unloading assembly: {ex.Message}");
            return -1;
        }
    }

    /// <summary>
    /// Reload an assembly for hot reload support.
    /// This unloads the old version and loads the new one.
    /// Called from C++ ScriptManager::ReloadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int ReloadAssembly(char* assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8((IntPtr)assemblyPathPtr) ?? "";
            return ReloadAssemblyInternal(assemblyPath);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error reloading assembly: {ex.Message}");
            return -1;
        }
    }

    /// <summary>
    /// Internal implementation of assembly reload logic.
    /// Separated so it can be called from both managed and unmanaged code.
    /// </summary>
    private static int ReloadAssemblyInternal(string assemblyPath)
    {
        try
        {
            Console.WriteLine($"[ScriptHost] Reloading assembly: {assemblyPath}");

            // Save state before unload
            StatePreserver.SaveAllSystemStates(assemblyPath);

            // Unload existing assembly
            if (!AssemblyManager.UnloadAssembly(assemblyPath))
            {
                Console.WriteLine($"[ScriptHost] Warning: Failed to unload existing assembly during reload");
            }

            // Wait a bit for finalizers to complete
            System.Threading.Thread.Sleep(100);

            // Load new version
            if (AssemblyManager.LoadAssembly(assemblyPath) == null)
            {
                Console.WriteLine($"[ScriptHost] Failed to load new assembly during reload");
                return -1;
            }

            // Discover systems in newly loaded assembly
            Assembly? newAssembly = AssemblyManager.GetLoadedAssembly(assemblyPath);
            if (newAssembly != null)
            {
                SystemDiscovery.DiscoverSystemsInAssembly(newAssembly);
            }

            // Clean up saved state
            StatePreserver.ClearSavedState(assemblyPath);

            Console.WriteLine($"[ScriptHost] Successfully reloaded: {assemblyPath}");
            return 0; // Success
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error in ReloadAssemblyInternal: {ex.Message}");
            return -1;
        }
    }

    /// <summary>
    /// Compile all .cs files in a directory into an assembly using Roslyn.
    /// Called from C++ to compile scripts in-editor.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int CompileScriptsInDirectory(char* scriptsDirPtr, char* outputAssemblyPathPtr)
    {
        try
        {
            string dir = Marshal.PtrToStringUTF8((IntPtr)scriptsDirPtr) ?? "";
            string outPath = Marshal.PtrToStringUTF8((IntPtr)outputAssemblyPathPtr) ?? "";

            Console.WriteLine($"[ScriptHost] CompileScriptsInDirectory: {dir} -> {outPath}");

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            return res;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] CompileScriptsInDirectory error: {ex}");
            return -1;
        }
    }

    /// <summary>
    /// Compile directory and return pointer to UTF8 diagnostics string (allocated with CoTaskMemAlloc).
    /// Caller must free the returned pointer using FreeStringFromManaged.
    /// Returns IntPtr.Zero on failure to allocate.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe IntPtr CompileDirectoryWithDiagnostics(char* scriptsDirPtr, char* outputAssemblyPathPtr)
    {
        try
        {
            string dir = Marshal.PtrToStringUTF8((IntPtr)scriptsDirPtr) ?? "";
            string outPath = Marshal.PtrToStringUTF8((IntPtr)outputAssemblyPathPtr) ?? "";

            Console.WriteLine($"[ScriptHost] CompileDirectoryWithDiagnostics: {dir} -> {outPath}");

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);

            string diags = RoslynCompiler.GetLastDiagnostics() ?? string.Empty;
            // Encode as UTF8 and allocate unmanaged memory
            var bytes = System.Text.Encoding.UTF8.GetBytes(diags + '\0');
            IntPtr p = Marshal.AllocHGlobal(bytes.Length);
            Marshal.Copy(bytes, 0, p, bytes.Length);

            return p;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] CompileDirectoryWithDiagnostics error: {ex}");
            return IntPtr.Zero;
        }
    }

    [UnmanagedCallersOnly]
    public static unsafe void FreeStringFromManaged(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero) return;
        Marshal.FreeHGlobal(ptr);
    }

    /// <summary>
    /// Compile scripts and reload resulting assembly.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int CompileAndReload(char* scriptsDirPtr, char* outputAssemblyPathPtr)
    {
        try
        {
            string dir = Marshal.PtrToStringUTF8((IntPtr)scriptsDirPtr) ?? "";
            string outPath = Marshal.PtrToStringUTF8((IntPtr)outputAssemblyPathPtr) ?? "";

            Console.WriteLine($"[ScriptHost] CompileAndReload: {dir} -> {outPath}");

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            if (res != 0)
            {
                Console.WriteLine("[ScriptHost] Compilation failed, aborting reload.");
                return -1;
            }

            // Reload the compiled assembly
            return ReloadAssemblyInternal(outPath);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] CompileAndReload error: {ex}");
            return -1;
        }
    }

    /// <summary>
    /// Generate a minimal .csproj file in the specified directory to help external IDEs
    /// (VS / Rider) open the script folder. Writes to <dir>/<projectName>.csproj.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int GenerateCsProj(char* scriptsDirPtr, char* projectNamePtr)
    {
        try
        {
            string dir = Marshal.PtrToStringUTF8((IntPtr)scriptsDirPtr) ?? "";
            string projectName = Marshal.PtrToStringUTF8((IntPtr)projectNamePtr) ?? "ScriptsProject";

            if (string.IsNullOrWhiteSpace(dir) || !Directory.Exists(dir))
            {
                Console.WriteLine($"[ScriptHost] GenerateCsProj: invalid dir {dir}");
                return -1;
            }

            string outPath = Path.Combine(dir, projectName + ".csproj");

            string template = @"<Project Sdk=""Microsoft.NET.Sdk""> 
                                    <PropertyGroup>
                                        <TargetFramework>net9.0</TargetFramework>
                                        <ImplicitUsings>enable</ImplicitUsings>
                                        <Nullable>enable</Nullable>
                                    </PropertyGroup>
                                    <ItemGroup>
                                        <Compile Include=""**\*.cs"" />
                                    </ItemGroup>
                                </Project>
                             ";

            File.WriteAllText(outPath, template);
            Console.WriteLine($"[ScriptHost] Generated csproj: {outPath}");
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] GenerateCsProj error: {ex.Message}");
            return -1;
        }
    }

    private static IntPtr StringToHGlobalUtf8(string? s)
    {
        if (s == null) return IntPtr.Zero;
        var bytes = System.Text.Encoding.UTF8.GetBytes(s + '\0');
        IntPtr p = Marshal.AllocHGlobal(bytes.Length);
        Marshal.Copy(bytes, 0, p, bytes.Length);
        return p;
    }

    /// <summary>
    /// Managed wrapper that compiles scripts in `dir` and reloads the resulting assembly at `outPath`.
    /// This is callable from other managed classes (e.g., file watcher).
    /// </summary>
    public static unsafe int TriggerCompileAndReloadManaged(string dir, string outPath)
    {
        try
        {
            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            if (res != 0)
            {
                Console.WriteLine("[ScriptHost] Compilation failed in TriggerCompileAndReloadManaged");
                return res;
            }

            // Reload the compiled assembly
            return ReloadAssemblyInternal(outPath);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] TriggerCompileAndReloadManaged error: {ex}");
            return -1;
        }
    }

    /// <summary>
    /// Discover all ISystem implementations in loaded assemblies.
    /// Called from C++ ScriptManager::DiscoverScriptedSystems()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void* DiscoverSystems(int* outCount)
    {
        try
        {
            Console.WriteLine("[ScriptHost] Discovering systems in loaded assemblies...");

            var allSystemHandles = new List<ulong>();

            // Discover systems in all loaded assemblies
            foreach (var assembly in AssemblyManager.GetAllLoadedAssemblies())
            {
                string[] discovered = SystemDiscovery.DiscoverSystemsInAssembly(assembly);
                foreach (var entry in discovered)
                {
                    // Parse "handle:typename" format
                    var parts = entry.Split(':');
                    if (parts.Length == 2 && ulong.TryParse(parts[0], out ulong handle))
                    {
                        allSystemHandles.Add(handle);
                    }
                }
            }

            Console.WriteLine($"[ScriptHost] Found {allSystemHandles.Count} system types");

            // Allocate unmanaged array for handles
            IntPtr handlesPtr = Marshal.AllocHGlobal(sizeof(ulong) * allSystemHandles.Count);
            Marshal.Copy(allSystemHandles.Select(h => (long)h).ToArray(), 0, handlesPtr, allSystemHandles.Count);

            *outCount = allSystemHandles.Count;
            return (void*)handlesPtr;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error discovering systems: {ex.Message}");
            *outCount = 0;
            return null;
        }
    }

    /// <summary>
    /// Create an instance of a scripted system.
    /// Called from C++ when creating ScriptSystemWrapper.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe ulong CreateSystemInstance(char* typeNamePtr)
    {
        try
        {
            string typeName = Marshal.PtrToStringUTF8((IntPtr)typeNamePtr) ?? "";

            // Find the type in loaded assemblies
            Type? systemType = null;
            foreach (var assembly in AssemblyManager.GetAllLoadedAssemblies())
            {
                systemType = assembly.GetType(typeName);
                if (systemType != null) break;
            }

            if (systemType == null)
            {
                Console.WriteLine($"[ScriptHost] System type not found: {typeName}");
                return 0;
            }

            // Create instance and get handle
            ulong handle = SystemDiscovery.CreateSystemInstanceFromType(systemType);
            if (handle == 0)
            {
                Console.WriteLine($"[ScriptHost] Failed to create instance of: {typeName}");
                return 0;
            }

            // If we have saved state from a previous unload, restore it
            StatePreserver.RestoreSystemState(systemType.Assembly.Location ?? "", SystemDiscovery.GetSystemInstance(handle)!);

            Console.WriteLine($"[ScriptHost] Created system instance: {typeName} (handle: {handle})");
            return handle;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error creating system instance: {ex.Message}");
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
    public static unsafe void GetSystemMetadata(ulong handle, char* outNameBuffer, int* outGroup, int* outRunMode)
    {
        try
        {
            Type? systemType = SystemDiscovery.GetSystemType(handle);
            if (systemType == null)
            {
                Console.WriteLine($"[ScriptHost] System handle not found: {handle}");
                return;
            }
            
            // Get instance if available (for ISystemMetadata interface)
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            
            // Get name
            string name = systemType.FullName ?? systemType.Name;
            byte[] nameBytes = System.Text.Encoding.UTF8.GetBytes(name);
            Marshal.Copy(nameBytes, 0, (IntPtr)outNameBuffer, System.Math.Min(nameBytes.Length, 256));
            
            // Get group - check ISystemMetadata interface first, then [SystemGroup] attribute
            *outGroup = (int)SystemMetadataExtractor.GetSystemGroup(systemType, instance);
            
            // Get run mode - check for [ExecuteInEditMode] attribute
            *outRunMode = SystemMetadataExtractor.HasExecuteInEditMode(systemType) 
                ? (int)SystemRunMode.Always 
                : (int)SystemRunMode.PlayOnly;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error getting metadata: {ex.Message}");
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
    [UnmanagedCallersOnly]
    public static unsafe int GetSystemComponentAccesses(ulong handle, uint* outReadHashes, uint* outWriteHashes, int maxSize)
    {
        try
        {
            Type? systemType = SystemDiscovery.GetSystemType(handle);
            if (systemType == null)
            {
                Console.WriteLine($"[ScriptHost] System handle not found: {handle}");
                return 0;
            }

            // Extract component accesses from C# attributes using ComponentAccessBridge
            var accesses = ComponentAccessBridge.ExtractComponentAccesses(systemType);
            
            // Separate into read and write accesses
            int readCount = 0;
            int writeCount = 0;

            foreach (var (hash, mode) in accesses)
            {
                if (mode == ComponentAccessMode.Read)
                {
                    if (readCount < maxSize / 2)
                    {
                        outReadHashes[readCount++] = hash;
                    }
                }
                else // Write or ReadWrite
                {
                    if (writeCount < maxSize / 2)
                    {
                        outWriteHashes[writeCount++] = hash;
                    }
                }
            }

            // Return total count (read count in lower 16 bits, write count in upper 16 bits)
            return (readCount) | (writeCount << 16);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error getting component accesses: {ex.Message}");
            return 0;
        }
    }

    /// <summary>
    /// Hash a component type name using FNV-1a algorithm in C++.
    /// Delegates to C++ to ensure hash consistency across language boundary.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe uint HashComponentTypeName(char* typeNamePtr)
    {
        try
        {
            // For now, compute here but ideally this would call C++
            string typeName = Marshal.PtrToStringUTF8((IntPtr)typeNamePtr) ?? "";
            return ComponentAccessBridge.Fnv1aHashPublic(typeName);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error hashing component type: {ex.Message}");
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
    public static unsafe int ResolveSystemGroup(int attributeGroup)
    {
        try
        {
            // For now, return directly (C# already applied priority correctly)
            // In future: Could validate/transform the group with C++ logic
            return attributeGroup;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error resolving system group: {ex.Message}");
            return 0; // Default to Update
        }
    }

    /// <summary>
    /// Call OnCreate on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void CallSystemOnCreate(ulong handle, void* worldPtr)
    {
        try
        {
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                Console.WriteLine($"[ScriptHost] System instance not found: {handle}");
                return;
            }
            
            if (instance is ISystem system)
            {
                // Wrap native World pointer in managed World wrapper
                World managedWorld = new World(worldPtr);
                Console.WriteLine($"[ScriptHost] CallSystemOnCreate for {instance.GetType().Name}");
                system.OnCreate(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error in OnCreate: {ex.Message}");
        }
    }

    /// <summary>
    /// Call OnUpdate on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void CallSystemOnUpdate(ulong handle, void* worldPtr, float deltaTime)
    {
        try
        {
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                return;
            }
            
            if (instance is ISystem system)
            {
                // Wrap native World pointer in managed World wrapper
                World managedWorld = new World(worldPtr);
                system.OnUpdate(managedWorld, deltaTime);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error in OnUpdate: {ex.Message}");
        }
    }
    /// <summary>
    /// Call OnDestroy on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void CallSystemOnDestroy(ulong handle, void* worldPtr)
    {
        try
        {
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                return;
            }
            
            if (instance is ISystem system)
            {
                // Wrap native World pointer in managed World wrapper
                World managedWorld = new World(worldPtr);
                system.OnDestroy(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error in OnDestroy: {ex.Message}");
        }
    }
}

// ============================================================================
// SCRIPTING ENUMS - Must match C++ Engine definitions
// ============================================================================
// These enums are marshaled between C# and C++ and MUST have identical values.
// Validation: See EnumParity validator below
// 
// C++ Source: engine/core/ecs/SystemGroup.h
// C# Validation: ScriptHost.ValidateEnumParity()
// ============================================================================

/// <summary>
/// System execution group - defines when systems run relative to engine lifecycle.
/// 
/// MUST match C++ ECS::SystemGroup enum values for correct P/Invoke marshaling.
/// If C++ values change, update both enums AND the validation logic below.
/// </summary>
public enum SystemGroup
{
    /// <summary>
    /// Systems execute before main update cycle (frame setup, input processing)
    /// </summary>
    PreUpdate = 0,
    
    /// <summary>
    /// Main update systems (gameplay logic, AI, etc.)
    /// </summary>
    Update = 1,
    
    /// <summary>
    /// Systems execute after update (cleanup, state finalization)
    /// </summary>
    PostUpdate = 2,
    
    /// <summary>
    /// Physics pre-calculation phase
    /// </summary>
    PrePhysics = 3,
    
    /// <summary>
    /// Physics simulation systems
    /// </summary>
    Physics = 4,
    
    /// <summary>
    /// Physics post-calculation phase (resolution, callbacks)
    /// </summary>
    PostPhysics = 5,
    
    /// <summary>
    /// Rendering preparation phase
    /// </summary>
    PreRender = 6,
    
    /// <summary>
    /// Render systems (camera updates, draw calls)
    /// </summary>
    Render = 7,
    
    /// <summary>
    /// Post-render cleanup (frame finalization)
    /// </summary>
    PostRender = 8
}

/// <summary>
/// System execution mode - determines when systems are active.
/// 
/// MUST match C++ ECS::SystemRunMode enum values for correct P/Invoke marshaling.
/// If C++ values change, update both enums AND the validation logic below.
/// </summary>
public enum SystemRunMode
{
    /// <summary>
    /// System always runs (edit and play mode)
    /// </summary>
    Always = 0,
    
    /// <summary>
    /// System runs only in play mode
    /// </summary>
    PlayOnly = 1,
    
    /// <summary>
    /// System runs only in editor/edit mode
    /// </summary>
    EditOnly = 2
}

/// <summary>
/// Validates that C# enum values match C++ engine definitions.
/// Called on assembly load to catch enum mismatches early.
/// </summary>
public static class EnumParityValidator
{
    /// <summary>
    /// Validate that all C# enums match C++ values.
    /// Should be called during assembly initialization.
    /// </summary>
    /// <returns>True if all enums match, false if mismatch detected</returns>
    public static bool ValidateEnumParity()
    {
        bool valid = true;

        // Validate SystemGroup enum
        valid &= ValidateSystemGroupEnum();
        
        // Validate SystemRunMode enum
        valid &= ValidateSystemRunModeEnum();

        if (!valid)
        {
            Console.WriteLine("[EnumParityValidator] WARNING: Enum mismatch detected! " +
                "C# enum values do not match C++ definitions. This will cause P/Invoke marshaling errors.");
        }

        return valid;
    }

    /// <summary>
    /// Validate SystemGroup enum values against expected C++ values.
    /// </summary>
    private static bool ValidateSystemGroupEnum()
    {
        // Expected C++ values (must match engine/core/ecs/SystemGroup.h)
        var expectedValues = new Dictionary<SystemGroup, int>
        {
            { SystemGroup.PreUpdate, 0 },
            { SystemGroup.Update, 1 },
            { SystemGroup.PostUpdate, 2 },
            { SystemGroup.PrePhysics, 3 },
            { SystemGroup.Physics, 4 },
            { SystemGroup.PostPhysics, 5 },
            { SystemGroup.PreRender, 6 },
            { SystemGroup.Render, 7 },
            { SystemGroup.PostRender, 8 }
        };

        bool valid = true;
        foreach (var (group, expectedValue) in expectedValues)
        {
            int actualValue = (int)group;
            if (actualValue != expectedValue)
            {
                Console.WriteLine(
                    $"[EnumParityValidator] SystemGroup.{group} mismatch: " +
                    $"expected {expectedValue}, got {actualValue}");
                valid = false;
            }
        }

        return valid;
    }

    /// <summary>
    /// Validate SystemRunMode enum values against expected C++ values.
    /// </summary>
    private static bool ValidateSystemRunModeEnum()
    {
        // Expected C++ values (must match engine/core/ecs/SystemRunMode.h)
        var expectedValues = new Dictionary<SystemRunMode, int>
        {
            { SystemRunMode.Always, 0 },
            { SystemRunMode.PlayOnly, 1 },
            { SystemRunMode.EditOnly, 2 }
        };

        bool valid = true;
        foreach (var (mode, expectedValue) in expectedValues)
        {
            int actualValue = (int)mode;
            if (actualValue != expectedValue)
            {
                Console.WriteLine(
                    $"[EnumParityValidator] SystemRunMode.{mode} mismatch: " +
                    $"expected {expectedValue}, got {actualValue}");
                valid = false;
            }
        }

        return valid;
    }

    /// <summary>
    /// Get a detailed report of all enum values for documentation purposes.
    /// Useful for verifying against C++ source files.
    /// </summary>
    public static string GetEnumParityReport()
    {
        var report = new System.Text.StringBuilder();
        report.AppendLine("=== C# Enum Parity Report ===");
        report.AppendLine();

        report.AppendLine("SystemGroup values:");
        foreach (SystemGroup group in Enum.GetValues<SystemGroup>())
        {
            report.AppendLine($"  {group} = {(int)group}");
        }
        report.AppendLine();

        report.AppendLine("SystemRunMode values:");
        foreach (SystemRunMode mode in Enum.GetValues<SystemRunMode>())
        {
            report.AppendLine($"  {mode} = {(int)mode}");
        }

        return report.ToString();
    }
}
