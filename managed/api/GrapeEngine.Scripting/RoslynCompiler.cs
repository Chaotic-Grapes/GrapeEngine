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

using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace GrapeEngine.Scripting;

internal static class RoslynCompiler
{
    private static string s_lastDiagnostics = string.Empty;

    public static unsafe int CompileDirectoryToAssembly(string dirPath, string outputAssemblyPath, IEnumerable<string>? references = null)
    {
        s_lastDiagnostics = string.Empty;
        try
        {
            if (!Directory.Exists(dirPath))
            {
                Console.WriteLine($"[RoslynCompiler] Directory not found: {dirPath}");
                return -1;
            }

            var csFiles = Directory.GetFiles(dirPath, "*.cs", SearchOption.AllDirectories);
            if (csFiles.Length == 0)
            {
                Console.WriteLine($"[RoslynCompiler] No .cs files found in {dirPath}");
                return -1;
            }

            var syntaxTrees = new List<SyntaxTree>();
            foreach (var file in csFiles)
            {
                var src = File.ReadAllText(file);
                syntaxTrees.Add(CSharpSyntaxTree.ParseText(src, path: file));
            }

            var assemblyName = Path.GetFileNameWithoutExtension(outputAssemblyPath);

            // Default references: mscorlib, System, System.Core, netstandard and current ScriptHost assembly
            var refs = new List<MetadataReference>();

            // Add commonly available references
            var trustedAssemblies = (AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") as string)?.Split(Path.PathSeparator) ?? Array.Empty<string>();
            foreach (var asmPath in trustedAssemblies)
            {
                var name = Path.GetFileName(asmPath);
                if (name.StartsWith("System") || name.StartsWith("mscorlib") || name.StartsWith("netstandard") || name.StartsWith("Microsoft.CSharp") || name.StartsWith("System.Private.CoreLib"))
                {
                    try { refs.Add(MetadataReference.CreateFromFile(asmPath)); } catch { }
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
                    if (File.Exists(r)) refs.Add(MetadataReference.CreateFromFile(r));
                }
            }

            var compilation = CSharpCompilation.Create(
                assemblyName,
                syntaxTrees: syntaxTrees,
                references: refs.Distinct(),
                options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary, optimizationLevel: OptimizationLevel.Debug)
            );

            // Emit to file
            using (var fs = new FileStream(outputAssemblyPath, FileMode.Create))
            {
                var emitResult = compilation.Emit(fs);
                if (!emitResult.Success)
                {
                    var sb = new System.Text.StringBuilder();
                    foreach (var diag in emitResult.Diagnostics)
                    {
                        var line = $"{diag.Id}: {diag.GetMessage()} (at {diag.Location})";
                        Console.WriteLine($"[RoslynCompiler] {line}");
                        sb.AppendLine(line);
                    }
                    s_lastDiagnostics = sb.ToString();
                    return -1;
                }
            }

            Console.WriteLine($"[RoslynCompiler] Compilation succeeded: {outputAssemblyPath}");
            s_lastDiagnostics = "";
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[RoslynCompiler] Compilation error: {ex}");
            s_lastDiagnostics = ex.ToString();
            return -1;
        }
    }

    public static string GetLastDiagnostics()
    {
        return s_lastDiagnostics ?? string.Empty;
    }
}
