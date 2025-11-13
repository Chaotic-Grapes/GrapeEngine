/* Start Header *****************************************************************/
/*!
\file   Vector2.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
Defines the Vector2 struct for 2D vectors.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Numerics;

// ============================================================================
// Math Types - MUST match C++ memory layout exactly for marshaling
// ============================================================================
// NOTE: Do NOT use System.Numerics types for marshaling!
// It caused a memory corruption the last time I tried!
// ============================================================================

/// <summary>
/// 2D Vector
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector2(float x, float y)
{
    public float X = x, Y = y;

    /// <summary>
    /// Zero vector (0, 0)
    /// </summary>
    public static Vector2 Zero => new(0, 0);

    /// <summary>
    /// One vector (1, 1)
    /// </summary>
    public static Vector2 One => new(1, 1);

    /// <summary>
    /// Up vector (0, 1)
    /// </summary>
    public static Vector2 Up => new(0, 1);

    /// <summary>
    /// Down vector (0, -1)
    /// </summary>
    public static Vector2 Down => new(0, -1);

    /// <summary>
    /// Left vector (-1, 0)
    /// </summary>
    public static Vector2 Left => new(-1, 0);

    /// <summary>
    /// Right vector (1, 0)
    /// </summary>
    public static Vector2 Right => new(1, 0);

    public static Vector2 operator +(Vector2 a, Vector2 b) => new(a.X + b.X, a.Y + b.Y);
    public static Vector2 operator -(Vector2 a, Vector2 b) => new(a.X - b.X, a.Y - b.Y);
    public static Vector2 operator *(Vector2 a, float scalar) => new(a.X * scalar, a.Y * scalar);
    public static Vector2 operator /(Vector2 a, float scalar) => new(a.X / scalar, a.Y / scalar);

    /// <summary>
    /// The magnitude (length) of the vector.
    /// </summary>
    public readonly float Magnitude => MathF.Sqrt(X * X + Y * Y);

    /// <summary>
    /// The normalized (unit length) vector.
    /// </summary>
    public readonly Vector2 Normalized => this / Magnitude;

    public readonly override string ToString() => $"({X}, {Y})";
}