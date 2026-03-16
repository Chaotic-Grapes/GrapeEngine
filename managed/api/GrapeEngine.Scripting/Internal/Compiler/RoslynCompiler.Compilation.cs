using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Text;
using System.Reflection;
using GrapeEngine.Scripting.Services;
using System.Globalization;

namespace GrapeEngine.Scripting.Internal.Compiler;

internal static partial class RoslynCompiler
{
    public static int CompileDirectoryToAssembly(string dirPath, string outputAssemblyPath,
        IEnumerable<string>? references = null)
    {
        _lastDiagnosticsList.Clear();

        try
        {
            if (!Directory.Exists(dirPath))
            {
                Logging.LogInternal($"Directory not found: {dirPath}", LogLevel.Warning);
                return -1;
            }

            // Gather all .cs files (exclude build artifacts)
            string[] csFiles = Directory.GetFiles(dirPath, "*.cs", SearchOption.AllDirectories)
                .Where(f => !f.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") &&
                            !f.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
                .ToArray();
            if (csFiles.Length == 0)
            {
                Logging.LogInternal($"No .cs files found in {dirPath}", LogLevel.Warning);
                return -1;
            }

            var syntaxTrees = new List<SyntaxTree>();
            foreach (string file in csFiles)
            {
                SourceText sourceText = SourceText.From(File.ReadAllText(file), System.Text.Encoding.UTF8);
                syntaxTrees.Add(CSharpSyntaxTree.ParseText(sourceText, path: file));
            }

            string assemblyName = Path.GetFileNameWithoutExtension(outputAssemblyPath);
            var refs = new List<MetadataReference>();

            string[] trustedAssemblies =
                (AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") as string)?.Split(Path.PathSeparator) ?? [];
            foreach (string asmPath in trustedAssemblies)
            {
                try
                {
                    refs.Add(MetadataReference.CreateFromFile(asmPath));
                }
                catch (Exception ex) when (IsRecoverableCompilerException(ex))
                {
                    // Skip any assemblies that can't be loaded.
                }
            }

            try
            {
                Assembly? scriptApi = Assembly.Load("GrapeEngine.Scripting");
                if (scriptApi != null && !string.IsNullOrEmpty(scriptApi.Location))
                {
                    refs.Add(MetadataReference.CreateFromFile(scriptApi.Location));
                }
            }
            catch (Exception ex) when (IsRecoverableCompilerException(ex))
            {
            }

            if (references != null)
            {
                refs.AddRange(from r in references
                              where File.Exists(r)
                              select MetadataReference.CreateFromFile(r));
            }

            CSharpCompilation compilation = CSharpCompilation.Create(
                assemblyName,
                syntaxTrees: syntaxTrees,
                references: refs.Distinct(),
                options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                    optimizationLevel: OptimizationLevel.Debug));

            using var ms = new MemoryStream();
            using var pdbStream = new MemoryStream();

            var emitResult = compilation.Emit(ms, pdbStream);
            if (!emitResult.Success)
            {
                _lastDiagnosticsList.Clear();

                foreach (Diagnostic diagnostic in emitResult.Diagnostics)
                {
                    if (diagnostic.Severity != DiagnosticSeverity.Error &&
                        diagnostic.Severity != DiagnosticSeverity.Warning)
                    {
                        continue;
                    }

                    ReportDiagnostic(diagnostic);
                    var location = diagnostic.Location.GetLineSpan();
                    string filePath = location.Path;
                    int lineNumber = location.StartLinePosition.Line + 1;
                    int columnNumber = location.StartLinePosition.Character + 1;
                    string line =
                        $"{filePath}({lineNumber},{columnNumber}): {diagnostic.Id}: {diagnostic.GetMessage(CultureInfo.InvariantCulture)}";
                    _lastDiagnosticsList.Add(line);
                }

                return -1;
            }

            try
            {
                byte[] bytes = ms.ToArray();
                byte[] pdbBytes = pdbStream.ToArray();
                _lastCompiledPdbBytes = pdbBytes.Length > 0 ? pdbBytes : null;

                Logging.LogInternal($"[RoslynCompiler] Writing assembly to: {outputAssemblyPath}", LogLevel.Info);
                File.WriteAllBytes(outputAssemblyPath, bytes);
                Logging.LogInternal($"[RoslynCompiler] Wrote assembly: {outputAssemblyPath}", LogLevel.Info);

                if (pdbBytes is { Length: > 0 })
                {
                    string pdbPath = Path.ChangeExtension(outputAssemblyPath, ".pdb");
                    try
                    {
                        File.WriteAllBytes(pdbPath, pdbBytes);
                        Logging.LogInternal($"[RoslynCompiler] Wrote PDB: {pdbPath}", LogLevel.Debug);
                    }
                    catch (Exception pdbEx) when (IsRecoverableCompilerException(pdbEx))
                    {
                        Logging.LogInternal($"[RoslynCompiler] Warning: Failed to write PDB: {pdbEx.Message}", LogLevel.Warning);
                    }
                }

                _lastCompiledTempAssemblyPath = outputAssemblyPath;
            }
            catch (Exception ex) when (IsRecoverableCompilerException(ex))
            {
                _lastDiagnosticsList.Clear();
                string errMsg = $"Failed to write assembly {outputAssemblyPath}:\n{ex}";
                _lastDiagnosticsList.Add(errMsg);
                try
                {
                    Logging.LogInternal(errMsg, LogLevel.Error);
                }
                catch (Exception logEx) when (IsRecoverableCompilerException(logEx))
                {
                }

                return -1;
            }

            _lastDiagnosticsList.Clear();
            return 0;
        }
        catch (Exception ex) when (IsRecoverableCompilerException(ex))
        {
            Logging.LogInternal($"Compilation error: {ex}", LogLevel.Error);
            _lastDiagnosticsList.Clear();
            _lastDiagnosticsList.Add(ex.ToString());
            try
            {
                Logging.LogInternal(ex.ToString(), LogLevel.Error);
            }
            catch (Exception logEx) when (IsRecoverableCompilerException(logEx))
            {
            }

            return -1;
        }
    }

    public static string GetLastCompiledTempAssemblyPath()
    {
        return _lastCompiledTempAssemblyPath;
    }

    public static byte[]? GetLastCompiledPdbBytes()
    {
        return _lastCompiledPdbBytes;
    }
}
