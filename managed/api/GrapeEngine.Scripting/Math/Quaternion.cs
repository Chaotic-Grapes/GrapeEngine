/* Start Header *****************************************************************/
/*!
\file   Quaternion.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
Defines the Quaternion struct for 3D rotations. Direct conversion from C++.

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
/// Quaternion for rotations.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Quaternion(float x, float y, float z, float w)
{
    public float X = x, Y = y, Z = z, W = w;

    public static Quaternion Identity => new(0, 0, 0, 1);
    public readonly override string ToString() => $"({X}, {Y}, {Z}, {W})";

    public readonly float SquareLength() => X * X + Y * Y + Z * Z + W * W;

    public readonly float Length() => MathF.Sqrt(SquareLength());

    public void Normalize()
    {
        var len = Length();
        if (len > 0f)
        {
            var inv = 1f / len;
            X *= inv; Y *= inv; Z *= inv; W *= inv;
        }
        else
        {
            X = Y = Z = 0f; W = 1f;
        }
    }

    public readonly Quaternion Normalized()
    {
        var len = Length();
        if (len > 0f)
        {
            var inv = 1f / len;
            return new Quaternion(X * inv, Y * inv, Z * inv, W * inv);
        }
        return Identity;
    }

    public readonly Quaternion Conjugate() => new(-X, -Y, -Z, W);

    public readonly Quaternion Inverse()
    {
        var sq = SquareLength();
        if (sq > 0f)
        {
            var inv = 1f / sq;
            var c = Conjugate();
            return new Quaternion(c.X * inv, c.Y * inv, c.Z * inv, c.W * inv);
        }
        return Identity;
    }

    public static Quaternion Multiply(Quaternion a, Quaternion b)
    {
        // Hamilton product: q = a * b
        return new Quaternion(
            a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,
            a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
            a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W,
            a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z
        );
    }

    public static Quaternion FromAxisAngle(Vector3 axis, float angleRadians)
    {
        // ensure axis normalized
        var nx = axis.X; var ny = axis.Y; var nz = axis.Z;
        var mag = GMath.Sqrt(nx * nx + ny * ny + nz * nz);
        if (mag > 0f)
        {
            nx /= mag; ny /= mag; nz /= mag;
        }

        var half = angleRadians * 0.5f;
        var s = GMath.Sin(half);
        var c = GMath.Cos(half);
        return new Quaternion(nx * s, ny * s, nz * s, c);
    }

    public static Quaternion FromEulerRad(float pitchX, float yawY, float rollZ)
    {
        // Uses yaw (Y), pitch (X), roll (Z) convention
        var hx = pitchX * 0.5f;
        var hy = yawY * 0.5f;
        var hz = rollZ * 0.5f;

        var sx = GMath.Sin(hx); var cx = GMath.Cos(hx);
        var sy = GMath.Sin(hy); var cy = GMath.Cos(hy);
        var sz = GMath.Sin(hz); var cz = GMath.Cos(hz);

        var w = cz * cx * cy + sz * sx * sy;
        var x = sz * cx * cy - cz * sx * sy;
        var y = cz * sx * cy + sz * cx * sy;
        var z = cz * cx * sy - sz * sx * cy;

        return new Quaternion(x, y, z, w);
    }

    public static Quaternion Slerp(Quaternion a, Quaternion b, float t)
    {
        // Clamp t
        if (t <= 0f) return a;
        if (t >= 1f) return b;

        var dot = a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;

        // If the dot product is negative, slerp won't take the shorter path.
        // Fix by reversing one quaternion.
        if (dot < 0f)
        {
            b = new Quaternion(-b.X, -b.Y, -b.Z, -b.W);
            dot = -dot;
        }

        const float DOT_THRESHOLD = 0.9995f;
        if (dot > DOT_THRESHOLD)
        {
            // If quaternions are very close, use linear interpolation and normalize
            var rx = a.X + t * (b.X - a.X);
            var ry = a.Y + t * (b.Y - a.Y);
            var rz = a.Z + t * (b.Z - a.Z);
            var rw = a.W + t * (b.W - a.W);
            var outQ = new Quaternion(rx, ry, rz, rw);
            outQ.Normalize();
            return outQ;
        }

        var theta0 = GMath.Acos(GMath.Clamp(dot, -1f, 1f)); // angle between input
        var theta = theta0 * t;
        var sinTheta = GMath.Sin(theta);
        var sinTheta0 = GMath.Sin(theta0);

        var s0 = GMath.Cos(theta) - dot * sinTheta / sinTheta0;
        var s1 = sinTheta / sinTheta0;

        return new Quaternion(
            (s0 * a.X) + (s1 * b.X),
            (s0 * a.Y) + (s1 * b.Y),
            (s0 * a.Z) + (s1 * b.Z),
            (s0 * a.W) + (s1 * b.W)
        );
    }

    public readonly Vector3 Rotate(Vector3 v) // rotate vector by this quaternion
    {
        var qv = new Quaternion(v.X, v.Y, v.Z, 0f);
        var res = Multiply(Multiply(this, qv), Conjugate());
        return new Vector3(res.X, res.Y, res.Z);
    }

    public static Quaternion operator *(Quaternion a, Quaternion b) => Multiply(a, b);

    public static bool operator ==(Quaternion a, Quaternion b)
        => a.X == b.X && a.Y == b.Y && a.Z == b.Z && a.W == b.W;

    public static bool operator !=(Quaternion a, Quaternion b) => !(a == b);

    public override readonly bool Equals(object? obj)
    {
        return obj is Quaternion q && this == q;
    }

    public override readonly int GetHashCode()
    {
        return HashCode.Combine(X, Y, Z, W);
    }
}

