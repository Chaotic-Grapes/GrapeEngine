/* Start Header *****************************************************************/
/*!
\file   MathAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
P/Invoke declarations for the Math API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Math API.
/// </summary>
internal partial class MathAPI
{
    // Random
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int RandomInt(int min, int max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomFloat")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float RandomFloat(float min, float max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomIntSeeded")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int RandomIntSeeded(int min, int max, uint seed);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomFloatSeeded")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float RandomFloatSeeded(float min, float max, uint seed);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomDouble")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double RandomDouble(double min, double max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomDoubleSeeded")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double RandomDoubleSeeded(double min, double max, uint seed);

    // Random - Additional Integral Types
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomByte")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial byte RandomByte(byte min, byte max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomSByte")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial sbyte RandomSByte(sbyte min, sbyte max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomShort")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial short RandomShort(short min, short max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomUShort")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ushort RandomUShort(ushort min, ushort max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomUInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial uint RandomUInt(uint min, uint max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomLong")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial nint RandomLong(nint min, nint max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomULong")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial nuint RandomULong(nuint min, nuint max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomLong64")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial long RandomLong64(long min, long max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RandomULong64")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ulong RandomULong64(ulong min, ulong max);

    // Clamp/Lerp
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Clamp")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Clamp(float value, float min, float max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampDouble")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double ClampDouble(double value, double min, double max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int ClampInt(int value, int min, int max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampByte")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial byte ClampByte(byte value, byte min, byte max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampSByte")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial sbyte ClampSByte(sbyte value, sbyte min, sbyte max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampShort")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial short ClampShort(short value, short min, short max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampUShort")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ushort ClampUShort(ushort value, ushort min, ushort max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampUInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial uint ClampUInt(uint value, uint min, uint max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampLong")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial nint ClampLong(nint value, nint min, nint max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampULong")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial nuint ClampULong(nuint value, nuint min, nuint max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampLong64")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial long ClampLong64(long value, long min, long max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampULong64")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ulong ClampULong64(ulong value, ulong min, ulong max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Lerp")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Lerp(float a, float b, float t);

    // Basic Math
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Abs")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Abs(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Sqrt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Sqrt(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Pow")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Pow(float @base, float exponent);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Round")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Round(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Floor")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Floor(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Ceil")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Ceil(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Min")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Min(float a, float b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinDouble")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double MinDouble(double a, double b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Max")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Max(float a, float b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxDouble")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double MaxDouble(double a, double b);

    // Min/Max - Integral Types
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int MinInt(int a, int b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int MaxInt(int a, int b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinByte")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial byte MinByte(byte a, byte b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxByte")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial byte MaxByte(byte a, byte b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinSByte")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial sbyte MinSByte(sbyte a, sbyte b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxSByte")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial sbyte MaxSByte(sbyte a, sbyte b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinShort")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial short MinShort(short a, short b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxShort")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial short MaxShort(short a, short b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinUShort")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ushort MinUShort(ushort a, ushort b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxUShort")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ushort MaxUShort(ushort a, ushort b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinUInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial uint MinUInt(uint a, uint b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxUInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial uint MaxUInt(uint a, uint b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinLong")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial nint MinLong(nint a, nint b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxLong")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial nint MaxLong(nint a, nint b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinULong")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial nuint MinULong(nuint a, nuint b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxULong")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial nuint MaxULong(nuint a, nuint b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinLong64")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial long MinLong64(long a, long b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxLong64")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial long MaxLong64(long a, long b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MinULong64")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ulong MinULong64(ulong a, ulong b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_MaxULong64")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ulong MaxULong64(ulong a, ulong b);

    // Trigonometry
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Sin")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Sin(float angle);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Cos")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Cos(float angle);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Tan")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Tan(float angle);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Asin")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Asin(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Acos")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Acos(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Atan")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Atan(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Atan2")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Atan2(float y, float x);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_DegToRad")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float DegToRad(float degrees);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_RadToDeg")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float RadToDeg(float radians);

    // Vector2D operations
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Distance2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Distance2D(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_DistanceSquared2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float DistanceSquared2D(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Length2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Length2D(float x, float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_LengthSquared2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float LengthSquared2D(float x, float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Normalize2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Normalize2D(ref float x, ref float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Dot2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Dot2D(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Cross2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Cross2D(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Lerp2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Lerp2D(float x1, float y1, float x2, float y2, float t, out float outX, out float outY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_ClampVector2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void ClampVector2D(ref float x, ref float y, float maxLength);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_AngleTo")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float AngleTo(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_DirectionFromAngle")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void DirectionFromAngle(float angle, out float x, out float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Math_Rotate2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Rotate2D(ref float x, ref float y, float angle);
}
