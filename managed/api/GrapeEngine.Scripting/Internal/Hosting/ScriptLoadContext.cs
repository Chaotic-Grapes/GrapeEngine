using System.Reflection;
using System.Runtime.Loader;

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// Custom AssemblyLoadContext for hot reload support.
/// Allows assemblies to be unloaded and reloaded.
/// </summary>
internal sealed class ScriptLoadContext(string assemblyPath) : AssemblyLoadContext(isCollectible: true)
{
    private readonly AssemblyDependencyResolver _resolver = new(assemblyPath);

    protected override Assembly? Load(AssemblyName assemblyName)
    {
        // Prefer already-loaded assemblies in the default context to preserve
        // type identity for shared API assemblies (e.g. GrapeEngine.Scripting).
        var already = AppDomain.CurrentDomain.GetAssemblies()
            .FirstOrDefault(a => string.Equals(a.GetName().Name, assemblyName.Name, StringComparison.OrdinalIgnoreCase));
        if (already != null)
        {
            return already;
        }

        // Fallback to resolving within the custom load context.
        string? resolvedPath = _resolver.ResolveAssemblyToPath(assemblyName);
        return resolvedPath != null ? LoadFromAssemblyPath(resolvedPath) : null;
    }

    protected override IntPtr LoadUnmanagedDll(string unmanagedDllName)
    {
        string? libraryPath = _resolver.ResolveUnmanagedDllToPath(unmanagedDllName);
        return libraryPath != null ? LoadUnmanagedDllFromPath(libraryPath) : IntPtr.Zero;
    }
}
