using GrapeEngine.Math;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Components;


/// <summary>
/// Tag mask component: Bitmask for quick entity classification.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct TagMask(uint Mask);
