/* Start Header *****************************************************************/
/*!
\file   Vector3.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
Defines the Vector3 struct for 3D vectors. Direct conversion from C++.

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
/// 3D Vector
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector3(float x, float y, float z)
{
    public float X = x, Y = y, Z = z;

    /// <summary>
    /// Zero vector (0, 0, 0)
    /// </summary>
    public static Vector3 Zero => new(0, 0, 0);

    /// <summary>
    /// One vector (1, 1, 1)
    /// </summary>
    public static Vector3 One => new(1, 1, 1);

    /// <summary>
    /// Up vector (0, 1, 0)
    /// </summary>
    public static Vector3 Up => new(0, 1, 0);

    /// <summary>
    /// Down vector (0, -1, 0)
    /// </summary>
    public static Vector3 Down => new(0, -1, 0);

    /// <summary>
    /// Left vector (-1, 0, 0)
    /// </summary>
    public static Vector3 Left => new(-1, 0, 0);

    /// <summary>
    /// Right vector (1, 0, 0)
    /// </summary>
    public static Vector3 Right => new(1, 0, 0);

    /// <summary>
    /// Forward vector (0, 0, 1)
    /// </summary>
    public static Vector3 Forward => new(0, 0, 1);

    /// <summary>
    /// Back vector (0, 0, -1)
    /// </summary>
    public static Vector3 Back => new(0, 0, -1);

    public static Vector3 operator +(Vector3 a, Vector3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    public static Vector3 operator -(Vector3 a, Vector3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    public static Vector3 operator *(Vector3 a, float scalar) => new(a.X * scalar, a.Y * scalar, a.Z * scalar);
    public static Vector3 operator *(float scalar, Vector3 a) => new(a.X * scalar, a.Y * scalar, a.Z * scalar);
    public static Vector3 operator /(Vector3 a, float scalar) => new(a.X / scalar, a.Y / scalar, a.Z / scalar);

    /// <summary>
    /// The magnitude (length) of the vector.
    /// </summary>
    public readonly float Magnitude => MathF.Sqrt(X * X + Y * Y + Z * Z);

    /// <summary>
    /// The normalized (unit length) vector.
    /// </summary>
    public readonly Vector3 Normalized => this / Magnitude;

    public readonly override string ToString() => $"({X}, {Y}, {Z})";
}
