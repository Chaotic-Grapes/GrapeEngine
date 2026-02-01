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

using System;
using GrapeEngine.Math;
using GrapeEngine.Scripting.Internal.Unsafe;

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
    /// Generate a random int in the range [min, max].
    /// </summary>
    public static int Random(int min, int max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomInt(min, max);
    }

    /// <summary>
    /// Generate a random byte in the range [min, max].
    /// </summary>
    public static byte Random(byte min, byte max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomByte(min, max);
    }

    /// <summary>
    /// Generate a random sbyte in the range [min, max].
    /// </summary>
    public static sbyte Random(sbyte min, sbyte max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomSByte(min, max);
    }

    /// <summary>
    /// Generate a random short in the range [min, max].
    /// </summary>
    public static short Random(short min, short max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomShort(min, max);
    }

    /// <summary>
    /// Generate a random ushort in the range [min, max].
    /// </summary>
    public static ushort Random(ushort min, ushort max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomUShort(min, max);
    }

    /// <summary>
    /// Generate a random uint in the range [min, max].
    /// </summary>
    public static uint Random(uint min, uint max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomUInt(min, max);
    }

    /// <summary>
    /// Generate a random long in the range [min, max].
    /// </summary>
    public static long Random(long min, long max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomLong64(min, max);
    }

    /// <summary>
    /// Generate a random ulong in the range [min, max].
    /// </summary>
    public static ulong Random(ulong min, ulong max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomULong64(min, max);
    }

    /// <summary>
    /// Generate a random float in the range [min, max].
    /// </summary>
    public static float Random(float min, float max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomFloat(min, max);
    }

    /// <summary>
    /// Generate a random double in the range [min, max].
    /// </summary>
    public static double Random(double min, double max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomDouble(min, max);
    }

    /// <summary>
    /// Generate a random int with a seed.
    /// </summary>
    public static int Random(int min, int max, uint seed)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomIntSeeded(min, max, seed);
    }

    /// <summary>
    /// Generate a random float with a seed.
    /// </summary>
    public static float Random(float min, float max, uint seed)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomFloatSeeded(min, max, seed);
    }

    /// <summary>
    /// Generate a random double with a seed.
    /// </summary>
    public static double Random(double min, double max, uint seed)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.RandomDoubleSeeded(min, max, seed);
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
    /// Clamp a double value between min and max.
    /// </summary>
    public static double Clamp(double value, double min, double max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampDouble(value, min, max);
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
    /// Clamp a byte value between min and max.
    /// </summary>
    public static byte Clamp(byte value, byte min, byte max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampByte(value, min, max);
    }

    /// <summary>
    /// Clamp a signed byte value between min and max.
    /// </summary>
    public static sbyte Clamp(sbyte value, sbyte min, sbyte max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampSByte(value, min, max);
    }

    /// <summary>
    /// Clamp a short value between min and max.
    /// </summary>
    public static short Clamp(short value, short min, short max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampShort(value, min, max);
    }

    /// <summary>
    /// Clamp an unsigned short value between min and max.
    /// </summary>
    public static ushort Clamp(ushort value, ushort min, ushort max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampUShort(value, min, max);
    }

    /// <summary>
    /// Clamp an unsigned integer value between min and max.
    /// </summary>
    public static uint Clamp(uint value, uint min, uint max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampUInt(value, min, max);
    }

    /// <summary>
    /// Clamp a 64-bit long value between min and max.
    /// </summary>
    public static long Clamp(long value, long min, long max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampLong64(value, min, max);
    }

    /// <summary>
    /// Clamp an unsigned 64-bit long value between min and max.
    /// </summary>
    public static ulong Clamp(ulong value, ulong min, ulong max)
    {
        SwapMinMax(ref min, ref max);
        return MathAPI.ClampULong64(value, min, max);
    }

    /// <summary>
    /// Linearly interpolate between a and b by t.
    /// </summary>
    public static float Lerp(float a, float b, float t) => MathAPI.Lerp(a, b, t);

    /// <summary>
    /// Inverse linear interpolation between a and b for value (clamped to 0-1).
    /// </summary>
    public static float InverseLerp(float a, float b, float value)
    {
        float range = b - a;
        if (range == 0f)
            return 0f;

        return Clamp((value - a) / range, 0f, 1f);
    }

    /// <summary>
    /// Inverse linear interpolation between a and b for value (clamped to 0-1).
    /// </summary>
    public static double InverseLerp(double a, double b, double value)
    {
        double range = b - a;
        if (range == 0.0)
            return 0.0;

        return Clamp((value - a) / range, 0.0, 1.0);
    }

    /// <summary>
    /// Remap a value from an input range to an output range.
    /// </summary>
    public static float Remap(float inMin, float inMax, float outMin, float outMax, float value)
        => Lerp(outMin, outMax, InverseLerp(inMin, inMax, value));

    /// <summary>
    /// Remap a value from an input range to an output range.
    /// </summary>
    public static double Remap(double inMin, double inMax, double outMin, double outMax, double value)
        => outMin + (outMax - outMin) * InverseLerp(inMin, inMax, value);

    /// <summary>
    /// Move current toward target by maxDelta without overshooting.
    /// </summary>
    public static float MoveTowards(float current, float target, float maxDelta)
    {
        float delta = target - current;
        float absDelta = Abs(delta);
        if (absDelta <= maxDelta)
            return target;

        if (maxDelta < 0f)
            return current;

        return current + (delta / absDelta) * maxDelta;
    }

    /// <summary>
    /// Move current toward target by maxDelta without overshooting.
    /// </summary>
    public static double MoveTowards(double current, double target, double maxDelta)
    {
        double delta = target - current;
        double absDelta = Math.Abs(delta);
        if (absDelta <= maxDelta)
            return target;

        if (maxDelta < 0.0)
            return current;

        return current + (delta / absDelta) * maxDelta;
    }

    /// <summary>
    /// Smoothly interpolate between a and b using a Hermite curve.
    /// </summary>
    public static float SmoothStep(float a, float b, float t)
    {
        t = Clamp(t, 0f, 1f);
        t = t * t * (3f - 2f * t);
        return Lerp(a, b, t);
    }

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
    /// Get the minimum of two doubles.
    /// </summary>
    public static double Min(double a, double b) => MathAPI.MinDouble(a, b);

    /// <summary>
    /// Get the maximum of two values.
    /// </summary>
    public static float Max(float a, float b) => MathAPI.Max(a, b);

    /// <summary>
    /// Get the maximum of two doubles.
    /// </summary>
    public static double Max(double a, double b) => MathAPI.MaxDouble(a, b);

    /// <summary>
    /// Get the minimum of two integers.
    /// </summary>
    public static int Min(int a, int b) => MathAPI.MinInt(a, b);

    /// <summary>
    /// Get the maximum of two integers.
    /// </summary>
    public static int Max(int a, int b) => MathAPI.MaxInt(a, b);

    /// <summary>
    /// Get the minimum of two bytes.
    /// </summary>
    public static byte Min(byte a, byte b) => MathAPI.MinByte(a, b);

    /// <summary>
    /// Get the maximum of two bytes.
    /// </summary>
    public static byte Max(byte a, byte b) => MathAPI.MaxByte(a, b);

    /// <summary>
    /// Get the minimum of two signed bytes.
    /// </summary>
    public static sbyte Min(sbyte a, sbyte b) => MathAPI.MinSByte(a, b);

    /// <summary>
    /// Get the maximum of two signed bytes.
    /// </summary>
    public static sbyte Max(sbyte a, sbyte b) => MathAPI.MaxSByte(a, b);

    /// <summary>
    /// Get the minimum of two shorts.
    /// </summary>
    public static short Min(short a, short b) => MathAPI.MinShort(a, b);

    /// <summary>
    /// Get the maximum of two shorts.
    /// </summary>
    public static short Max(short a, short b) => MathAPI.MaxShort(a, b);

    /// <summary>
    /// Get the minimum of two unsigned shorts.
    /// </summary>
    public static ushort Min(ushort a, ushort b) => MathAPI.MinUShort(a, b);

    /// <summary>
    /// Get the maximum of two unsigned shorts.
    /// </summary>
    public static ushort Max(ushort a, ushort b) => MathAPI.MaxUShort(a, b);

    /// <summary>
    /// Get the minimum of two unsigned integers.
    /// </summary>
    public static uint Min(uint a, uint b) => MathAPI.MinUInt(a, b);

    /// <summary>
    /// Get the maximum of two unsigned integers.
    /// </summary>
    public static uint Max(uint a, uint b) => MathAPI.MaxUInt(a, b);

    /// <summary>
    /// Get the minimum of two 64-bit longs.
    /// </summary>
    public static long Min(long a, long b) => MathAPI.MinLong64(a, b);

    /// <summary>
    /// Get the maximum of two 64-bit longs.
    /// </summary>
    public static long Max(long a, long b) => MathAPI.MaxLong64(a, b);

    /// <summary>
    /// Get the minimum of two unsigned 64-bit longs.
    /// </summary>
    public static ulong Min(ulong a, ulong b) => MathAPI.MinULong64(a, b);

    /// <summary>
    /// Get the maximum of two unsigned 64-bit longs.
    /// </summary>
    public static ulong Max(ulong a, ulong b) => MathAPI.MaxULong64(a, b);

    /// <summary>
    /// Get the sign of an integer (-1, 0, or 1).
    /// </summary>
    public static int Sign(int value) => value > 0 ? 1 : (value < 0 ? -1 : 0);

    /// <summary>
    /// Get the sign of a float (-1, 0, or 1).
    /// </summary>
    public static float Sign(float value) => value > 0f ? 1f : (value < 0f ? -1f : 0f);

    /// <summary>
    /// Get the sign of a double (-1, 0, or 1).
    /// </summary>
    public static double Sign(double value) => value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0);

    /// <summary>
    /// Compare two floats within a tolerance.
    /// </summary>
    public static bool Approximately(float a, float b, float epsilon = Epsilon) => Abs(a - b) <= epsilon;

    /// <summary>
    /// Compare two doubles within a tolerance.
    /// </summary>
    public static bool Approximately(double a, double b, double epsilon = Epsilon) => Math.Abs(a - b) <= epsilon;

    /// <summary>
    /// Compute e raised to the specified power.
    /// </summary>
    public static float Exp(float value) => MathF.Exp(value);

    /// <summary>
    /// Compute the natural logarithm.
    /// </summary>
    public static float Log(float value) => MathF.Log(value);

    /// <summary>
    /// Compute the base-10 logarithm.
    /// </summary>
    public static float Log10(float value) => MathF.Log10(value);

    /// <summary>
    /// Compute the floating-point remainder of value/length.
    /// </summary>
    public static float Fmod(float value, float length) => length == 0f ? 0f : value % length;

    /// <summary>
    /// Compute the floating-point remainder of value/length.
    /// </summary>
    public static double Fmod(double value, double length) => length == 0.0 ? 0.0 : value % length;

    /// <summary>
    /// Compute the integer remainder of value/length.
    /// </summary>
    public static int Mod(int value, int length) => length == 0 ? 0 : value % length;

    /// <summary>
    /// Round up to the nearest integer.
    /// </summary>
    public static int CeilToInt(float value) => (int)MathF.Ceiling(value);

    /// <summary>
    /// Round down to the nearest integer.
    /// </summary>
    public static int FloorToInt(float value) => (int)MathF.Floor(value);

    /// <summary>
    /// Round to the nearest integer.
    /// </summary>
    public static int RoundToInt(float value) => (int)MathF.Round(value);

    /// <summary>
    /// Check if a float is finite (not NaN or Infinity).
    /// </summary>
    public static bool IsFinite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

    /// <summary>
    /// Check if a double is finite (not NaN or Infinity).
    /// </summary>
    public static bool IsFinite(double value) => !double.IsNaN(value) && !double.IsInfinity(value);

    /// <summary>
    /// Check if a float is NaN.
    /// </summary>
    public static bool IsNaN(float value) => float.IsNaN(value);

    /// <summary>
    /// Check if a double is NaN.
    /// </summary>
    public static bool IsNaN(double value) => double.IsNaN(value);

    /// <summary>
    /// Check if a float is Infinity.
    /// </summary>
    public static bool IsInfinity(float value) => float.IsInfinity(value);

    /// <summary>
    /// Check if a double is Infinity.
    /// </summary>
    public static bool IsInfinity(double value) => double.IsInfinity(value);

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

    /// <summary>
    /// Repeat a value so it stays within [0, length).
    /// </summary>
    public static float Repeat(float value, float length)
    {
        if (length == 0f)
            return 0f;

        return value - MathAPI.Floor(value / length) * length;
    }

    /// <summary>
    /// Ping-pong a value so it oscillates between 0 and length.
    /// </summary>
    public static float PingPong(float value, float length)
    {
        if (length == 0f)
            return 0f;

        float repeated = Repeat(value, length * 2f);
        return length - Abs(repeated - length);
    }

    /// <summary>
    /// Calculate the shortest difference between two angles in degrees.
    /// </summary>
    public static float DeltaAngle(float fromDeg, float toDeg)
    {
        float delta = Repeat(toDeg - fromDeg, 360f);
        return delta > 180f ? delta - 360f : delta;
    }

    /// <summary>
    /// Wrap an angle in radians to [-Pi, Pi].
    /// </summary>
    public static float WrapAngleRad(float radians)
    {
        float wrapped = Repeat(radians + Pi, Tau);
        return wrapped - Pi;
    }

    /// <summary>
    /// Wrap an angle in degrees to [-180, 180].
    /// </summary>
    public static float WrapAngleDeg(float degrees)
    {
        float wrapped = Repeat(degrees + 180f, 360f);
        return wrapped - 180f;
    }

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
    /// Clamp a vector's magnitude to a minimum and maximum length.
    /// </summary>
    public static Vector2 ClampMagnitude(Vector2 v, float minLength, float maxLength)
    {
        if (minLength > maxLength)
        {
            float temp = minLength;
            minLength = maxLength;
            maxLength = temp;
        }

        float length = Length(v);
        if (length == 0f)
            return v;

        if (length < minLength)
            return v * (minLength / length);

        if (length > maxLength)
            return v * (maxLength / length);

        return v;
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

    /// <summary>
    /// Project a onto onto.
    /// </summary>
    public static Vector2 Project(Vector2 a, Vector2 onto)
    {
        float denom = Dot(onto, onto);
        if (denom <= Epsilon)
            return Vector2.Zero;

        return onto * (Dot(a, onto) / denom);
    }

    /// <summary>
    /// Reflect v across a normal.
    /// </summary>
    public static Vector2 Reflect(Vector2 v, Vector2 normal)
    {
        float denom = Dot(normal, normal);
        if (denom <= Epsilon)
            return v;

        return v - normal * (2f * Dot(v, normal) / denom);
    }

    /// <summary>
    /// Get the left-hand perpendicular of a vector.
    /// </summary>
    public static Vector2 Perp(Vector2 v) => new(-v.Y, v.X);

    /// <summary>
    /// Get the angle (in radians) from the positive X axis.
    /// </summary>
    public static float Angle(Vector2 v) => Atan2(v.Y, v.X);

    /// <summary>
    /// Move current toward target by maxDelta without overshooting.
    /// </summary>
    public static Vector2 MoveTowards(Vector2 current, Vector2 target, float maxDelta)
    {
        Vector2 delta = new(target.X - current.X, target.Y - current.Y);
        float dist = Length(delta);
        if (dist <= maxDelta || dist <= Epsilon)
            return target;

        return current + delta * (maxDelta / dist);
    }

    /// <summary>
    /// Generate a random boolean.
    /// </summary>
    public static bool Random() => Random(0, 1) == 1;

    /// <summary>
    /// Generate a random Vector2 within a min/max range.
    /// </summary>
    public static Vector2 Random(Vector2 min, Vector2 max)
        => new(Random(min.X, max.X), Random(min.Y, max.Y));

    /// <summary>
    /// Generate a random Vector2 inside the unit circle.
    /// </summary>
    public static Vector2 RandomInsideUnitCircle()
    {
        float angle = Random(0f, Tau);
        float radius = MathAPI.Sqrt(Random(0f, 1f)); // sqrt for uniform area distribution
        return new Vector2(MathAPI.Cos(angle) * radius, MathAPI.Sin(angle) * radius);
    }

    // Helper to swap min and max if min > max
    private static void SwapMinMax(ref int min, ref int max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref float min, ref float max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref double min, ref double max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref byte min, ref byte max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref sbyte min, ref sbyte max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref short min, ref short max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref ushort min, ref ushort max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref uint min, ref uint max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref long min, ref long max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }

    private static void SwapMinMax(ref ulong min, ref ulong max)
    {
        if (min > max)
        {
            var temp = min;
            min = max;
            max = temp;
        }
    }
}

