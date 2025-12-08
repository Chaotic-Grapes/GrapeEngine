namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// Implement on managed systems to preserve state across hot-reloads.
/// </summary>
public interface IHotReloadable
{
    /// <summary>
    /// Called before the current assembly is unloaded. Return a serialized blob
    /// representing the instance state. The host will pass the blob back into
    /// the new instance after reload via <see cref="OnAfterReload"/>.
    /// </summary>
    byte[]? OnBeforeUnload();

    /// <summary>
    /// Called on the newly-created instance after reload with the previous
    /// instance's serialized state (if any).
    /// </summary>
    /// <param name="data">Serialized state blob from previous instance.</param>
    void OnAfterReload(byte[]? data);
}

