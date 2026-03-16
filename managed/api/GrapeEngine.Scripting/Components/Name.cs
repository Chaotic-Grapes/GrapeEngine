using GrapeEngine.Math;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Components;


/// <summary>
/// Name component: String identifier for an entity.
/// Uses StringId for unmanaged, blittable storage.
/// 
/// Usage:
/// <code>
/// entity.AddComponent(new Name(Strings.Intern("Player")));
/// string name = Strings.Resolve(entity.GetComponent&lt;Name&gt;().Value);
/// </code>
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Name
{
    /// <summary>
    /// The interned string identifier.
    /// Use Strings.Intern() to create, Strings.Resolve() to read.
    /// </summary>
    public StringId Value;

    /// <summary>
    /// Create a Name component with an interned string.
    /// </summary>
    public Name(StringId value)
    {
        Value = value;
    }

    public override readonly string ToString() => Strings.Resolve(Value) ?? "<null>";
}
