using GrapeEngine.Math;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Components;


/// <summary>
/// Prefab instance metadata component: Runtime data for prefab instances.
/// Contains the hash of the source prefab and instance flags.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct PrefabInstanceMetadata
{
    public uint PrefabHash;
    public uint Flags;

    public PrefabInstanceMetadata(uint hash, uint flags = 0)
    {
        PrefabHash = hash;
        Flags = flags;
    }
}
