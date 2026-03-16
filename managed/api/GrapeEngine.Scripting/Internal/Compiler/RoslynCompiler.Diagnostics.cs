using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.Text;
using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Services;
using System.Globalization;

namespace GrapeEngine.Scripting.Internal.Compiler;

internal static partial class RoslynCompiler
{
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
        if (index < 0 || index >= _lastDiagnosticsList.Count)
        {
            return null;
        }

        return _lastDiagnosticsList[index];
    }

    /// <summary>
    /// Report a single diagnostic to the native side with structured data.
    /// O(1) processing per diagnostic with cached line maps.
    /// </summary>
    private static void ReportDiagnostic(Diagnostic diagnostic)
    {
        if (!diagnostic.Location.IsInSource)
        {
            return;
        }

        SyntaxTree? tree = diagnostic.Location.SourceTree;
        if (tree == null)
        {
            return;
        }

        if (!_lineCache.TryGetValue(tree, out SourceText? sourceText))
        {
            sourceText = tree.GetText();
            _lineCache[tree] = sourceText;
        }

        TextSpan span = diagnostic.Location.SourceSpan;
        LinePosition linePos = sourceText.Lines.GetLinePosition(span.Start);

        byte severity = diagnostic.Severity switch
        {
            DiagnosticSeverity.Warning => 1,
            DiagnosticSeverity.Error => 2,
            _ => 0
        };

        try
        {
            DebugAPI.ScriptDiagnostic(
                diagnostic.Id,
                severity,
                tree.FilePath ?? "Unknown",
                linePos.Line + 1,
                linePos.Character + 1,
                diagnostic.GetMessage(CultureInfo.InvariantCulture));
        }
        catch (Exception ex) when (IsRecoverableCompilerException(ex))
        {
            Logging.LogInternal($"Failed to report diagnostic: {ex.Message}", LogLevel.Warning);
        }
    }
}
