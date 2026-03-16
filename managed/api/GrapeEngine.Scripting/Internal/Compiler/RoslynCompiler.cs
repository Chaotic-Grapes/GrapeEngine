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
using Microsoft.CodeAnalysis.Text;
using System.Collections.Generic;

namespace GrapeEngine.Scripting.Internal.Compiler;

internal static partial class RoslynCompiler
{
    // Cache line maps per file for O(1) lookup of line/column information
    private static readonly Dictionary<SyntaxTree, SourceText> _lineCache = [];

    // Store diagnostics as a list of individual messages for summary purposes
    private static readonly List<string> _lastDiagnosticsList = [];

    // Store the temporary assembly path after compilation (C++ moves it after unload)
    private static string _lastCompiledTempAssemblyPath = "";

    // Store the last compiled PDB bytes for retrieval
    private static byte[]? _lastCompiledPdbBytes = null;


    private static bool IsRecoverableCompilerException(Exception ex)
    {
        return ex is not OutOfMemoryException and not StackOverflowException;
    }
}

