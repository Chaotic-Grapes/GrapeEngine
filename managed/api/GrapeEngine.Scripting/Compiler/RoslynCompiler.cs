/* Start Header *****************************************************************/
/*!
\file   RoslynCompiler.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Simple Roslyn compilation helper used by ScriptHost to compile C# scripts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using System.Reflection;
using GrapeEngine.Scripting.Hosting;

namespace GrapeEngine.Scripting.Compiler;

internal static class RoslynCompiler
{
    // Store diagnostics as a list of individual messages so callers can
    // choose how to present them (UI can enumerate, logs can summarize).
    private readonly static List<string> _lastDiagnosticsList = [];
    
    // Store the path to the last compiled assembly (may be versioned)
    private static string _lastCompiledAssemblyPath = "";
    
    // Store the last compiled PDB bytes for retrieval
    private static byte[]? _lastCompiledPdbBytes = null;
    
    // Track compilation count: 0 = initial/boot compilation, 1+ = hot reload
    // First compilation writes to GameScripts.dll directly
    // Subsequent compilations write to GameScripts_hotreload_N.dll for hot reload safety
    private static int _compilationCount = 0;

    public static int CompileDirectoryToAssembly(string dirPath, string outputAssemblyPath, IEnumerable<string>? references = null)
    {
        _lastDiagnosticsList.Clear();

        List<string> _errors = [];
        List<string> _warnings = [];

        try
        {
            if (!Directory.Exists(dirPath))
            {
                Logging.Log($"Directory not found: {dirPath}", LogLevel.Warning);
                return -1;
            }

            // Gather all .cs files (exclude build artifacts)
            var csFiles = Directory.GetFiles(dirPath, "*.cs", SearchOption.AllDirectories)
                .Where(f => !f.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && 
                            !f.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
                .ToArray();
            if (csFiles.Length == 0)
            {
                Logging.Log($"No .cs files found in {dirPath}", LogLevel.Warning);
                return -1;
            }

            var syntaxTrees = new List<SyntaxTree>();
            foreach (var file in csFiles)
            {
                // Read source code with encoding for PDB emission
                var sourceText = Microsoft.CodeAnalysis.Text.SourceText.From(
                    File.ReadAllText(file), 
                    System.Text.Encoding.UTF8);
                
                // Parse and add to syntax trees with encoding metadata
                syntaxTrees.Add(CSharpSyntaxTree.ParseText(sourceText, path: file));
            }

            var assemblyName = Path.GetFileNameWithoutExtension(outputAssemblyPath);

            // Default references: mscorlib, System, System.Core, netstandard and current ScriptHost assembly
            var refs = new List<MetadataReference>();

            // Add commonly available references
            var trustedAssemblies = (AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") as string)?.Split(Path.PathSeparator) ?? [];
            foreach (var asmPath in trustedAssemblies)
            {
                var name = Path.GetFileName(asmPath);
                if (name.StartsWith("System") || name.StartsWith("mscorlib") || name.StartsWith("netstandard") || name.StartsWith("Microsoft.CSharp") || name.StartsWith("System.Private.CoreLib"))
                {
                    try
                    {
                        refs.Add(MetadataReference.CreateFromFile(asmPath));
                    }
                    catch { }
                }
            }

            // Add reference to Scripting assembly if available
            try
            {
                var scriptApi = Assembly.Load("GrapeEngine.Scripting");
                if (scriptApi != null && !string.IsNullOrEmpty(scriptApi.Location))
                {
                    refs.Add(MetadataReference.CreateFromFile(scriptApi.Location));
                }
            }
            catch { }

            // Add user-provided references
            if (references != null)
            {
                foreach (var r in references)
                {
                    if (File.Exists(r))
                        refs.Add(MetadataReference.CreateFromFile(r));
                }
            }

            var compilation = CSharpCompilation.Create(
                assemblyName,
                syntaxTrees: syntaxTrees,
                references: refs.Distinct(),
                options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary, optimizationLevel: OptimizationLevel.Debug)
            );

            // Emit to an in-memory stream first so we only write an output assembly
            // if the compilation actually succeeds. This prevents producing a
            // corrupt/partial assembly when there are errors.
            using (var ms = new MemoryStream())
            using (var pdbStream = new MemoryStream())
            {
                var emitResult = compilation.Emit(ms, pdbStream);
                if (!emitResult.Success)
                {
                    _lastDiagnosticsList.Clear();
                    foreach (var diag in emitResult.Diagnostics)
                    {
                        var location = diag.Location.GetLineSpan();
                        var filePath = location.Path;
                        var lineNumber = location.StartLinePosition.Line + 1;
                        var columnNumber = location.StartLinePosition.Character + 1;

                        var line = $"{filePath}({lineNumber},{columnNumber}): {diag.Id}: {diag.GetMessage()}";

                        if (diag.Severity == DiagnosticSeverity.Error)
                            _errors.Add(line);
                        else if (diag.Severity == DiagnosticSeverity.Warning)
                            _warnings.Add(line);

                        _lastDiagnosticsList.Add(line);
                    }

                    // Emit each diagnostic as a separate managed log entry so the
                    // native ConsolePanel receives them as independent messages.
                    try
                    {
                        foreach (var line in _errors)
                        {
                            Logging.Log(line, LogLevel.Error);
                        }
                        foreach (var line in _warnings)
                        {
                            Logging.Log(line, LogLevel.Warning);
                        }
                    }
                    catch { }
                    // Do not write any output file on failure
                    return -1;
                }

                // Write the successful assembly to disk
                try
                {
                    var bytes = ms.ToArray();
                    var pdbBytes = pdbStream.ToArray();
                    
                    // Store PDB bytes for retrieval
                    _lastCompiledPdbBytes = pdbBytes.Length > 0 ? pdbBytes : null;
                    
                    string finalAssemblyPath;
                    
                    // First compilation (count=0): write directly to GameScripts.dll
                    // Subsequent compilations (count>0): use versioned names for hot reload safety
                    if (_compilationCount == 0)
                    {
                        // Initial boot compilation - write directly to the original path
                        Logging.LogInternal($"[RoslynCompiler] First compilation (boot) - writing directly to: {outputAssemblyPath}", LogLevel.Info);
                        
                        // Ensure the output directory exists
                        var dir = Path.GetDirectoryName(outputAssemblyPath) ?? "";
                        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
                        {
                            Directory.CreateDirectory(dir);
                        }
                        
                        // Write DLL directly to the original path
                        File.WriteAllBytes(outputAssemblyPath, bytes);
                        Logging.LogInternal($"[RoslynCompiler] Wrote initial assembly: {outputAssemblyPath}", LogLevel.Info);
                        
                        // Write PDB if provided
                        if (pdbBytes != null && pdbBytes.Length > 0)
                        {
                            string pdbPath = Path.ChangeExtension(outputAssemblyPath, ".pdb");
                            try
                            {
                                File.WriteAllBytes(pdbPath, pdbBytes);
                                Logging.LogInternal($"[RoslynCompiler] Wrote PDB: {pdbPath}", LogLevel.Debug);
                            }
                            catch (Exception pdbEx)
                            {
                                Logging.LogInternal($"[RoslynCompiler] Warning: Failed to write PDB: {pdbEx.Message}", LogLevel.Warning);
                            }
                        }
                        
                        finalAssemblyPath = outputAssemblyPath;
                    }
                    else
                    {
                        // Hot reload compilation (count>0): use versioned assembly loading
                        // Create GameScripts_hotreload_1.dll, GameScripts_hotreload_2.dll, etc.
                        // This avoids file-locking issues when unloading old versions.
                        Logging.LogInternal($"[RoslynCompiler] Hot reload compilation (count={_compilationCount}) - using versioned path", LogLevel.Info);
                        
                        string versionedPath = AssemblyManager.LoadVersionedAssembly(outputAssemblyPath, bytes, pdbBytes);
                        
                        if (versionedPath == null)
                        {
                            _lastDiagnosticsList.Clear();
                            string errMsg = $"Failed to write versioned assembly {outputAssemblyPath}.\nCheck AssemblyManager logs for details.";
                            _lastDiagnosticsList.Add(errMsg);
                            try 
                            {
                                Logging.Log(errMsg, LogLevel.Error);
                            }
                            catch { }
                            return -1;
                        }
                        
                        finalAssemblyPath = versionedPath;
                    }
                    
                    // Increment compilation counter after successful write
                    _compilationCount++;
                    
                    // Update the output path to point to the actual assembly
                    // The caller (TriggerCompileAndReloadManaged) will use this path to load the new version
                    string outputDir = Path.GetDirectoryName(outputAssemblyPath) ?? "";
                    
                    // Copy GrapeEngine.Scripting dependency to the same directory so it can be found at runtime
                    if (!string.IsNullOrEmpty(outputDir))
                    {
                        try
                        {
                            var scriptingAsm = Assembly.Load("GrapeEngine.Scripting");
                            if (scriptingAsm != null && !string.IsNullOrEmpty(scriptingAsm.Location))
                            {
                                var scriptingDllName = Path.GetFileName(scriptingAsm.Location);
                                var scriptingDstPath = Path.Combine(outputDir, scriptingDllName);
                                File.Copy(scriptingAsm.Location, scriptingDstPath, overwrite: true);
                                Logging.LogInternal($"Copied GrapeEngine.Scripting to output directory: {scriptingDstPath}", LogLevel.Info);
                            }
                        }
                        catch (Exception depEx)
                        {
                            Logging.Log($"Warning: Failed to copy GrapeEngine.Scripting dependency: {depEx.Message}", LogLevel.Warning);
                        }
                    }
                    
                    // Store the actual path (either original or versioned) for retrieval by the caller
                    _lastCompiledAssemblyPath = finalAssemblyPath;
                }
                catch (Exception ex)
                {
                    // If we fail to write the file, record an error with details
                    _lastDiagnosticsList.Clear();
                    string errMsg = $"Failed to write assembly {outputAssemblyPath}:\n{ex}";
                    _lastDiagnosticsList.Add(errMsg);
                    try 
                    {
                        Logging.Log(errMsg, LogLevel.Error);
                    }
                    catch { }
                    return -1;
                }
            }

            _lastDiagnosticsList.Clear();
            return 0;
        }
        catch (Exception ex)
        {
            Logging.Log($"Compilation error: {ex}", LogLevel.Error);
            _lastDiagnosticsList.Clear();
            _lastDiagnosticsList.Add(ex.ToString());
            try { Logging.Log(ex.ToString(), LogLevel.Error); } catch { }
            return -1;
        }
    }

    public static string GetLastCompiledAssemblyPath()
    {
        return _lastCompiledAssemblyPath;
    }

    public static string GetLastDiagnostics()
    {
        if (_lastDiagnosticsList == null || _lastDiagnosticsList.Count == 0)
            return string.Empty;
        return string.Join("\n", _lastDiagnosticsList);
    }

    // Expose diagnostics as indexed list accessors for native callers.
    public static int GetLastDiagnosticsCount()
    {
        return _lastDiagnosticsList?.Count ?? 0;
    }

    public static string? GetLastDiagnosticAt(int index)
    {
        if (_lastDiagnosticsList == null) return null;
        if (index < 0 || index >= _lastDiagnosticsList.Count) return null;
        return _lastDiagnosticsList[index];
    }

    public static byte[]? GetLastCompiledPdbBytes()
    {
        return _lastCompiledPdbBytes;
    }
} 
