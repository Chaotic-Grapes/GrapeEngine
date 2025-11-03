using System.Runtime.InteropServices;

namespace GrapeEngine.Numerics;

// ============================================================================
// Math Types - MUST match C++ memory layout exactly for marshaling
// ============================================================================
// NOTE: Do NOT use System.Numerics types for marshaling!
// It caused a memory corruption the last time I tried!
// ============================================================================

/// <summary>
/// Quaternion for rotations.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Quaternion(float x, float y, float z, float w)
{
    public float X = x, Y = y, Z = z, W = w;

    public static Quaternion Identity => new(0, 0, 0, 1);

    public readonly override string ToString() => $"({X}, {Y}, {Z}, {W})";
}