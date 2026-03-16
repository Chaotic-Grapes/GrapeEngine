using GrapeEngine.Math;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Components;


/// <summary>
/// Active component: Whether entity is active/enabled.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Active(bool Enabled);
