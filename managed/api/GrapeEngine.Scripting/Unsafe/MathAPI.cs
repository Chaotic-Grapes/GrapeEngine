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
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_RandomInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int RandomInt(int min, int max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_RandomFloat")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float RandomFloat(float min, float max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_RandomIntSeeded")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int RandomIntSeeded(int min, int max, uint seed);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_RandomFloatSeeded")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float RandomFloatSeeded(float min, float max, uint seed);

    // Clamp/Lerp
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Clamp")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Clamp(float value, float min, float max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_ClampInt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int ClampInt(int value, int min, int max);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Lerp")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Lerp(float a, float b, float t);

    // Basic Math
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Abs")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Abs(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Sqrt")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Sqrt(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Pow")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Pow(float @base, float exponent);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Round")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Round(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Floor")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Floor(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Ceil")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Ceil(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Min")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Min(float a, float b);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Max")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Max(float a, float b);

    // Trigonometry
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Sin")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Sin(float angle);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Cos")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Cos(float angle);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Tan")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Tan(float angle);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Asin")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Asin(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Acos")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Acos(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Atan")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Atan(float value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Atan2")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Atan2(float y, float x);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_DegToRad")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float DegToRad(float degrees);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_RadToDeg")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float RadToDeg(float radians);

    // Vector2D operations
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Distance2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Distance2D(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_DistanceSquared2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float DistanceSquared2D(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Length2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Length2D(float x, float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_LengthSquared2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float LengthSquared2D(float x, float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Normalize2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Normalize2D(ref float x, ref float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Dot2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Dot2D(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Cross2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float Cross2D(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Lerp2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Lerp2D(float x1, float y1, float x2, float y2, float t, out float outX, out float outY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_ClampVector2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void ClampVector2D(ref float x, ref float y, float maxLength);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_AngleTo")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float AngleTo(float x1, float y1, float x2, float y2);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_DirectionFromAngle")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void DirectionFromAngle(float angle, out float x, out float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Math_Rotate2D")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Rotate2D(ref float x, ref float y, float angle);
}
