/* Start Header *****************************************************************/
/*!
\file   Vector4.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
Defines the Vector4 struct for 4D vectors. Direct conversion from C++.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Math;

// ============================================================================
// Math Types - MUST match C++ memory layout exactly for marshaling
// ============================================================================
// NOTE: Do NOT use System.Numerics types for marshaling!
// It caused a memory corruption the last time I tried!
// ============================================================================

/// <summary>
/// 4D Vector
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector4(float x, float y, float z, float w)
{
    public float X = x, Y = y, Z = z, W = w;

    /// <summary>
    /// Zero vector (0, 0, 0, 0)
    /// </summary>
    public static Vector4 Zero => new(0, 0, 0, 0);

    /// <summary>
    /// One vector (1, 1, 1, 1)
    /// </summary>
    public static Vector4 One => new(1, 1, 1, 1);

    public static Vector4 operator +(Vector4 a, Vector4 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W);
    public static Vector4 operator -(Vector4 a, Vector4 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W);
    public static Vector4 operator *(Vector4 a, float scalar) => new(a.X * scalar, a.Y * scalar, a.Z * scalar, a.W * scalar);
    public static Vector4 operator /(Vector4 a, float scalar) => new(a.X / scalar, a.Y / scalar, a.Z / scalar, a.W / scalar);

    public readonly override string ToString() => $"({X}, {Y}, {Z}, {W})";
}
