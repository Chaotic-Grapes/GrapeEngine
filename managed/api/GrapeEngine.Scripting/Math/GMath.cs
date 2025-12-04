/* Start Header *****************************************************************/
/*!
\file   GMath.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
Provides mathematical utility functions for game development.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Numerics;

namespace GrapeEngine.Math;

/// <summary>
/// Mathematical utility functions for game development.
/// </summary>
public static class GMath
{
    public const float Pi = 3.14159265359f;
    public const float Tau = 6.28318530718f;
    public const float Deg2Rad = 0.0174532925f;
    public const float Rad2Deg = 57.2957795131f;
    public const float Epsilon = 1e-6f;

    // ============================================================================
    // Random
    // ============================================================================

    /// <summary>
    /// Generate a random integer in the range [min, max].
    /// </summary>
    public static int RandomInt(int min, int max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomInt(min, max);
    }

    /// <summary>
    /// Generate a random float in the range [min, max].
    /// </summary>
    public static float RandomFloat(float min, float max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomFloat(min, max);
    }

    /// <summary>
    /// Generate a random integer with a seed.
    /// </summary>
    public static int RandomInt(int min, int max, uint seed)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomIntSeeded(min, max, seed);
    }

    /// <summary>
    /// Generate a random float with a seed.
    /// </summary>
    public static float RandomFloat(float min, float max, uint seed)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomFloatSeeded(min, max, seed);
    }

    // ============================================================================
    // Clamp / Lerp
    // ============================================================================

    /// <summary>
    /// Clamp a value between min and max.
    /// </summary>
    public static float Clamp(float value, float min, float max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.Clamp(value, min, max);
    }

    /// <summary>
    /// Clamp an integer value between min and max.
    /// </summary>
    public static int Clamp(int value, int min, int max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampInt(value, min, max);
    }

    /// <summary>
    /// Linearly interpolate between a and b by t.
    /// </summary>
    public static float Lerp(float a, float b, float t) => MathAPI.Lerp(a, b, t);

    // ============================================================================
    // Basic Math
    // ============================================================================

    /// <summary>
    /// Get the absolute value.
    /// </summary>
    public static float Abs(float value) => MathAPI.Abs(value);

    /// <summary>
    /// Get the square root.
    /// </summary>
    public static float Sqrt(float value) => MathAPI.Sqrt(value);

    /// <summary>
    /// Raise base to the power of exponent.
    /// </summary>
    public static float Pow(float baseValue, float exponent) => MathAPI.Pow(baseValue, exponent);

    /// <summary>
    /// Round to nearest integer.
    /// </summary>
    public static float Round(float value) => MathAPI.Round(value);

    /// <summary>
    /// Round down to nearest integer.
    /// </summary>
    public static float Floor(float value) => MathAPI.Floor(value);

    /// <summary>
    /// Round up to nearest integer.
    /// </summary>
    public static float Ceiling(float value) => MathAPI.Ceil(value);

    /// <summary>
    /// Get the minimum of two values.
    /// </summary>
    public static float Min(float a, float b) => MathAPI.Min(a, b);

    /// <summary>
    /// Get the maximum of two values.
    /// </summary>
    public static float Max(float a, float b) => MathAPI.Max(a, b);

    // ============================================================================
    // Trigonometry
    // ============================================================================

    /// <summary>
    /// Sine function (angle in radians).
    /// </summary>
    public static float Sin(float angle) => MathAPI.Sin(angle);

    /// <summary>
    /// Cosine function (angle in radians).
    /// </summary>
    public static float Cos(float angle) => MathAPI.Cos(angle);

    /// <summary>
    /// Tangent function (angle in radians).
    /// </summary>
    public static float Tan(float angle) => MathAPI.Tan(angle);

    /// <summary>
    /// Arcsine function (returns radians).
    /// </summary>
    public static float Asin(float value) => MathAPI.Asin(value);

    /// <summary>
    /// Arccosine function (returns radians).
    /// </summary>
    public static float Acos(float value) => MathAPI.Acos(value);

    /// <summary>
    /// Arctangent function (returns radians).
    /// </summary>
    public static float Atan(float value) => MathAPI.Atan(value);

    /// <summary>
    /// Two-argument arctangent (returns radians).
    /// </summary>
    public static float Atan2(float y, float x) => MathAPI.Atan2(y, x);

    /// <summary>
    /// Convert degrees to radians.
    /// </summary>
    public static float DegToRad(float degrees) => MathAPI.DegToRad(degrees);

    /// <summary>
    /// Convert radians to degrees.
    /// </summary>
    public static float RadToDeg(float radians) => MathAPI.RadToDeg(radians);

    // ============================================================================
    // Vector2 Operations
    // ============================================================================

    /// <summary>
    /// Calculate distance between two points.
    /// </summary>
    public static float Distance(Vector2 a, Vector2 b) => MathAPI.Distance2D(a.X, a.Y, b.X, b.Y);

    /// <summary>
    /// Calculate squared distance between two points (faster than Distance).
    /// </summary>
    public static float DistanceSquared(Vector2 a, Vector2 b) => MathAPI.DistanceSquared2D(a.X, a.Y, b.X, b.Y);

    /// <summary>
    /// Get the length of a vector.
    /// </summary>
    public static float Length(Vector2 v) => MathAPI.Length2D(v.X, v.Y);

    /// <summary>
    /// Get the squared length of a vector (faster than Length).
    /// </summary>
    public static float LengthSquared(Vector2 v) => MathAPI.LengthSquared2D(v.X, v.Y);

    /// <summary>
    /// Normalize a vector to unit length.
    /// </summary>
    public static Vector2 Normalize(Vector2 v)
    {
        float x = v.X, y = v.Y;
        MathAPI.Normalize2D(ref x, ref y);
        return new Vector2(x, y);
    }

    /// <summary>
    /// Dot product of two vectors.
    /// </summary>
    public static float Dot(Vector2 a, Vector2 b) => MathAPI.Dot2D(a.X, a.Y, b.X, b.Y);

    /// <summary>
    /// Cross product of two 2D vectors (returns scalar).
    /// </summary>
    public static float Cross(Vector2 a, Vector2 b) => MathAPI.Cross2D(a.X, a.Y, b.X, b.Y);

    /// <summary>
    /// Linearly interpolate between two vectors.
    /// </summary>
    public static Vector2 Lerp(Vector2 a, Vector2 b, float t)
    {
        MathAPI.Lerp2D(a.X, a.Y, b.X, b.Y, t, out float x, out float y);
        return new Vector2(x, y);
    }

    /// <summary>
    /// Clamp a vector's magnitude to a maximum length.
    /// </summary>
    public static Vector2 ClampMagnitude(Vector2 v, float maxLength)
    {
        float x = v.X, y = v.Y;
        MathAPI.ClampVector2D(ref x, ref y, maxLength);
        return new Vector2(x, y);
    }

    /// <summary>
    /// Get the angle (in radians) from vector a to vector b.
    /// </summary>
    public static float AngleTo(Vector2 from, Vector2 to) => MathAPI.AngleTo(from.X, from.Y, to.X, to.Y);

    /// <summary>
    /// Get a direction vector from an angle (in radians).
    /// </summary>
    public static Vector2 DirectionFromAngle(float angle)
    {
        MathAPI.DirectionFromAngle(angle, out float x, out float y);
        return new Vector2(x, y);
    }

    /// <summary>
    /// Rotate a vector by an angle (in radians).
    /// </summary>
    public static Vector2 Rotate(Vector2 v, float angle)
    {
        float x = v.X, y = v.Y;
        MathAPI.Rotate2D(ref x, ref y, angle);
        return new Vector2(x, y);
    }

    // Helper to swap min and max if min > max
    private static void SwapMinMax(ref int min, ref int max)
    {
        // Swap if min is greater than max
        if (min > max)
        {
            // Swap values with Min and Max
            var temp = min;

            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref float min, ref float max)
    {
        // Swap if min is greater than max
        if (min > max)
        {
            // Swap values with Min and Max
            var temp = min;

            min = max;
            max = temp;
        }
    }
}
