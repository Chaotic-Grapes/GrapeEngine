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
using Microsoft.CodeAnalysis.Text;
using System.Reflection;
using GrapeEngine.Scripting.Internal.Hosting;
using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Services;
using System.Globalization;

namespace GrapeEngine.Scripting.Internal.Compiler;

internal static class RoslynCompiler
{
    // Cache line maps per file for O(1) lookup of line/column information
    private static readonly Dictionary<SyntaxTree, SourceText> _lineCache = [];

    // Store diagnostics as a list of individual messages for summary purposes
    private static readonly List<string> _lastDiagnosticsList = [];

    // Store the temporary assembly path after compilation (C++ moves it after unload)
    private static string _lastCompiledTempAssemblyPath = "";

    // Store the last compiled PDB bytes for retrieval
    private static byte[]? _lastCompiledPdbBytes = null;

    public static int CompileDirectoryToAssembly(string dirPath, string outputAssemblyPath,
        IEnumerable<string>? references = null)
    {
        _lastDiagnosticsList.Clear();

        List<string> errors = [];
        List<string> warnings = [];

        try
        {
            if (!Directory.Exists(dirPath))
            {
                Logging.LogInternal($"Directory not found: {dirPath}", LogLevel.Warning);
                return -1;
            }

            // Gather all .cs files (exclude build artifacts)
            var csFiles = Directory.GetFiles(dirPath, "*.cs", SearchOption.AllDirectories)
                .Where(f => !f.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") &&
                            !f.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
                .ToArray();
            if (csFiles.Length == 0)
            {
                Logging.LogInternal($"No .cs files found in {dirPath}", LogLevel.Warning);
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

            // Add all trusted platform assemblies to ensure all system types are available
            // (List<>, Dictionary<>, etc. from System.Collections.Generic are critical)
            var trustedAssemblies =
                (AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") as string)?.Split(Path.PathSeparator) ?? [];
            foreach (var asmPath in trustedAssemblies)
            {
                try
                {
                    refs.Add(MetadataReference.CreateFromFile(asmPath));
                }
                catch
                {
                    // Skip any assemblies that can't be loaded
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
            catch
            {
            }

            // Add user-provided references
            if (references != null)
            {
                refs.AddRange(
                    from r
                        in references
                    where File.Exists(r)
                    select MetadataReference.CreateFromFile(r));
            }

            var compilation = CSharpCompilation.Create(
                assemblyName,
                syntaxTrees: syntaxTrees,
                references: refs.Distinct(),
                options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                    optimizationLevel: OptimizationLevel.Debug)
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

                    // Process each diagnostic individually for O(1) per diagnostic
                    foreach (var diagnostic in emitResult.Diagnostics)
                    {
                        if (diagnostic.Severity != DiagnosticSeverity.Error &&
                            diagnostic.Severity != DiagnosticSeverity.Warning)
                            continue;

                        // Report diagnostic to native side with structured data
                        ReportDiagnostic(diagnostic);

                        // Store for later summary access (for backward compatibility)
                        var location = diagnostic.Location.GetLineSpan();
                        var filePath = location.Path;
                        var lineNumber = location.StartLinePosition.Line + 1;
                        var columnNumber = location.StartLinePosition.Character + 1;
                        var line =
                            $"{filePath}({lineNumber},{columnNumber}): {diagnostic.Id}: {diagnostic.GetMessage(CultureInfo.InvariantCulture)}";
                        _lastDiagnosticsList.Add(line);
                    }

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

                    // Write to output assembly path directly
                    // (C++ handles temp->final moves during hot reload if needed)
                    Logging.LogInternal($"[RoslynCompiler] Writing assembly to: {outputAssemblyPath}", LogLevel.Info);

                    File.WriteAllBytes(outputAssemblyPath, bytes);
                    Logging.LogInternal($"[RoslynCompiler] Wrote assembly: {outputAssemblyPath}", LogLevel.Info);

                    // Write PDB alongside the assembly
                    if (pdbBytes is { Length: > 0 })
                    {
                        var pdbPath = Path.ChangeExtension(outputAssemblyPath, ".pdb");
                        try
                        {
                            File.WriteAllBytes(pdbPath, pdbBytes);
                            Logging.LogInternal($"[RoslynCompiler] Wrote PDB: {pdbPath}", LogLevel.Debug);
                        }
                        catch (Exception pdbEx)
                        {
                            Logging.LogInternal($"[RoslynCompiler] Warning: Failed to write PDB: {pdbEx.Message}",
                                LogLevel.Warning);
                        }
                    }

                    // Store the actual path for retrieval (same as output path)
                    _lastCompiledTempAssemblyPath = outputAssemblyPath;
                }
                catch (Exception ex)
                {
                    // If we fail to write the file, record an error with details
                    _lastDiagnosticsList.Clear();
                    var errMsg = $"Failed to write assembly {outputAssemblyPath}:\n{ex}";
                    _lastDiagnosticsList.Add(errMsg);
                    try
                    {
                        Logging.LogInternal(errMsg, LogLevel.Error);
                    }
                    catch
                    {
                    }

                    return -1;
                }
            }

            _lastDiagnosticsList.Clear();
            return 0;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"Compilation error: {ex}", LogLevel.Error);
            _lastDiagnosticsList.Clear();
            _lastDiagnosticsList.Add(ex.ToString());
            try
            {
                Logging.LogInternal(ex.ToString(), LogLevel.Error);
            }
            catch
            {
            }

            return -1;
        }
    }

    public static string GetLastCompiledTempAssemblyPath()
    {
        return _lastCompiledTempAssemblyPath;
    }

    public static string GetLastDiagnostics()
    {
        return _lastDiagnosticsList.Count == 0
            ? string.Empty
            : string.Join("\n", _lastDiagnosticsList);
    }

    // Expose diagnostics as indexed list accessors for native callers.
    public static int GetLastDiagnosticsCount()
    {
        return _lastDiagnosticsList?.Count ?? 0;
    }

    public static string? GetLastDiagnosticAt(int index)
    {
        if (index < 0 || index >= _lastDiagnosticsList.Count) return null;
        return _lastDiagnosticsList[index];
    }

    public static byte[]? GetLastCompiledPdbBytes()
    {
        return _lastCompiledPdbBytes;
    }

    /// <summary>
    /// Report a single diagnostic to the native side with structured data.
    /// O(1) processing per diagnostic with cached line maps.
    /// </summary>
    private static void ReportDiagnostic(Diagnostic diagnostic)
    {
        if (!diagnostic.Location.IsInSource)
            return;

        var tree = diagnostic.Location.SourceTree;
        if (tree == null)
            return;

        // Cache line map per file for O(1) lookup
        if (!_lineCache.TryGetValue(tree, out var sourceText))
        {
            sourceText = tree.GetText();
            _lineCache[tree] = sourceText;
        }

        var span = diagnostic.Location.SourceSpan;
        var linePos = sourceText.Lines.GetLinePosition(span.Start);

        // Convert severity to byte for P/Invoke
        byte severity = diagnostic.Severity switch
        {
            DiagnosticSeverity.Warning => 1, // Warning
            DiagnosticSeverity.Error => 2, // Error
            _ => 0 // Default/Hidden
        };

        try
        {
            DebugAPI.ScriptDiagnostic(
                diagnostic.Id,
                severity,
                tree.FilePath ?? "Unknown",
                linePos.Line + 1,
                linePos.Character + 1,
                diagnostic.GetMessage(CultureInfo.InvariantCulture)
            );
        }
        catch (Exception ex)
        {
            // If reporting fails, at least log it internally
            Logging.LogInternal($"Failed to report diagnostic: {ex.Message}", LogLevel.Warning);
        }
    }
}
