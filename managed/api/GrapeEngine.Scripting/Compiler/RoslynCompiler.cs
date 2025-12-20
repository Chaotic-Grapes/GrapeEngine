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
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Compiler;

internal static class RoslynCompiler
{
    // Store diagnostics as a list of individual messages so callers can
    // choose how to present them (UI can enumerate, logs can summarize).
    private readonly static List<string> _lastDiagnosticsList = [];

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

            // Gather all .cs files
            var csFiles = Directory.GetFiles(dirPath, "*.cs", SearchOption.AllDirectories);
            if (csFiles.Length == 0)
            {
                Logging.Log($"No .cs files found in {dirPath}", LogLevel.Warning);
                return -1;
            }

            var syntaxTrees = new List<SyntaxTree>();
            foreach (var file in csFiles)
            {
                // Read source code
                var src = File.ReadAllText(file);
                // Parse and add to syntax trees
                syntaxTrees.Add(CSharpSyntaxTree.ParseText(src, path: file));
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
            {
                var emitResult = compilation.Emit(ms);
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
                    File.WriteAllBytes(outputAssemblyPath, bytes);
                }
                catch
                {
                    // If we fail to write the file, record an error
                    _lastDiagnosticsList.Clear();
                    _lastDiagnosticsList.Add("Failed to write output assembly");
                    try 
                    { 
                        Logging.Log("Failed to write output assembly", LogLevel.Error); 
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
} 
