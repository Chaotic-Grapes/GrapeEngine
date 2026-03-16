/* Start Header *****************************************************************/
/*!
\file   AssemblyManager.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Manages assembly loading/unloading lifecycle.
*/
/* End Header *******************************************************************/

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// AssemblyManager - Encapsulates assembly loading and unloading logic.
/// 
/// Responsibilities:
/// - Load assemblies with custom AssemblyLoadContext for hot reload support
/// - Unload assemblies and free resources
/// - Track loaded assemblies and their contexts
/// - Handle assembly path resolution
/// </summary>
internal static partial class AssemblyManager
{
    /// <summary>
    /// Loaded assemblies and their load contexts (for hot reload support).
    /// Key: Assembly file path
    /// Value: (Assembly instance, Custom LoadContext for hot reload)
    /// </summary>
    private static readonly Dictionary<string, (Assembly Assembly, ScriptLoadContext? Context)> s_loadedAssemblies = new(StringComparer.OrdinalIgnoreCase);
    private static readonly object s_sync = new();

    private static bool IsRecoverableHostingException(Exception ex)
    {
        return ex is IOException
            or UnauthorizedAccessException
            or InvalidOperationException
            or ArgumentException
            or NotSupportedException
            or BadImageFormatException
            or FileLoadException
            or ReflectionTypeLoadException
            or TypeLoadException;
    }

    private static string NormalizeAssemblyKey(string assemblyPath)
    {
        if (string.IsNullOrWhiteSpace(assemblyPath))
            return assemblyPath;

        try
        {
            assemblyPath = Path.GetFullPath(assemblyPath);
        }
        catch (Exception ex) when (IsRecoverableHostingException(ex))
        {
            // Best-effort normalization; keep original string.
        }

        string dir = Path.GetDirectoryName(assemblyPath) ?? "";
        string filename = Path.GetFileNameWithoutExtension(assemblyPath);
        string ext = Path.GetExtension(assemblyPath);

        int hotreloadIndex = filename.LastIndexOf("_hotreload_", StringComparison.OrdinalIgnoreCase);
        if (hotreloadIndex > 0)
        {
            filename = filename.Substring(0, hotreloadIndex);
        }

        return Path.Combine(dir, filename + ext);
    }
}




