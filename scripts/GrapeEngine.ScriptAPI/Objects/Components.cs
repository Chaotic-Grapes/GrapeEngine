/* Start Header *****************************************************************/
/*!
\file   Components.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
Definitions for ECS component types used in the engine scripting API.

\details
This file contains the definitions for the various component types used in the ECS system.
These structs are used for marshaling data between managed and unmanaged code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Numerics;
using GrapeEngine.Math;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting;
public interface IComponentData
{
    void AddToEntity(Entity entity);
}

public readonly struct ComponentData<T>(T component) : IComponentData where T : unmanaged
{
    private readonly T _component = component;

    public void AddToEntity(Entity entity)
        => entity.AddComponent(_component);
}

// ============================================================================
// Component Mirrors - Must match C++ Components.h layout EXACTLY
// ============================================================================
// These structs mirror the C++ component definitions for marshaling.
// Memory layout must be identical for safe interop.
// ============================================================================

// ---------------------------------- Core Components ----------------------------------

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct Name
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
    public char[] Value;

    public Name(string value)
    {
        Value = new char[64];
        var chars = value.ToCharArray();
        Array.Copy(chars, Value, (int)GMath.Min(chars.Length, 63));
    }

    public override readonly string ToString() => new string(Value).TrimEnd('\0');
}

[StructLayout(LayoutKind.Sequential)]
public struct TagMask
{
    public uint Mask;
}

[StructLayout(LayoutKind.Sequential)]
public struct Active
{
    public bool Enabled;
}

[StructLayout(LayoutKind.Sequential)]
public struct PrefabLink
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
    public char[] PrefabPath;

    public PrefabLink(string path)
    {
        PrefabPath = new char[256];
        if (!string.IsNullOrEmpty(path))
        {
            var chars = path.ToCharArray();
            Array.Copy(chars, PrefabPath, (int)GMath.Min(chars.Length, 255));
        }
    }

    public readonly string GetPath() => new string(PrefabPath).TrimEnd('\0');
}

[StructLayout(LayoutKind.Sequential)]
public struct Lifetime
{
    public float Time;
}

// ---------------------------------- Transform Components ----------------------------------

[StructLayout(LayoutKind.Sequential)]
public struct Rotator
{
    public float RotationSpeed;
    public float RotationOffset;

    public Rotator(float speed, float offset = 0f)
    {
        RotationSpeed = speed;
        RotationOffset = offset;
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct LocalTransform
{
    public Vector3 Position;
    public Quaternion Rotation;
    public Vector3 Scale;

    public LocalTransform(Vector3 position, Quaternion rotation, Vector3 scale)
    {
        Position = position;
        Rotation = rotation;
        Scale = scale;
    }

    public static LocalTransform Default => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
}

[StructLayout(LayoutKind.Sequential)]
public struct WorldTransform
{
    public Matrix4x4 Matrix;
    public bool Dirty;
}

// ---------------------------------- 2D Physics Components ----------------------------------

[StructLayout(LayoutKind.Sequential)]
public struct LinearVelocity2D
{
    public Vector2 Value;

    public LinearVelocity2D(Vector2 value) => Value = value;
    public LinearVelocity2D(float x, float y) => Value = new Vector2(x, y);
}

[StructLayout(LayoutKind.Sequential)]
public struct Acceleration2D
{
    public Vector2 Value;

    public Acceleration2D(Vector2 value) => Value = value;
    public Acceleration2D(float x, float y) => Value = new Vector2(x, y);
}

[StructLayout(LayoutKind.Sequential)]
public struct AngularVelocity2D
{
    public float Value;

    public AngularVelocity2D(float value) => Value = value;
}

[StructLayout(LayoutKind.Sequential)]
public struct Rigidbody2D
{
    public float Mass;
    public float InverseMass;
    public float LinearDamping;
    public float AngularDamping;
    public float GravityScale;
    public uint Flags;

    // Flag bit positions
    public const uint FLAG_KINEMATIC = 1 << 0;
    public const uint FLAG_USE_GRAVITY = 1 << 1;
    public const uint FLAG_FIXED_ROTATION = 1 << 2;

    public bool IsKinematic
    {
        readonly get => (Flags & FLAG_KINEMATIC) != 0;
        set => Flags = value ? (Flags | FLAG_KINEMATIC) : (Flags & ~FLAG_KINEMATIC);
    }

    public bool UseGravity
    {
        readonly get => (Flags & FLAG_USE_GRAVITY) != 0;
        set => Flags = value ? (Flags | FLAG_USE_GRAVITY) : (Flags & ~FLAG_USE_GRAVITY);
    }

    public bool FixedRotation
    {
        readonly get => (Flags & FLAG_FIXED_ROTATION) != 0;
        set => Flags = value ? (Flags | FLAG_FIXED_ROTATION) : (Flags & ~FLAG_FIXED_ROTATION);
    }

    public static Rigidbody2D Dynamic(float mass = 1.0f)
    {
        return new Rigidbody2D
        {
            Mass = mass,
            InverseMass = mass > 0 ? 1f / mass : 0,
            LinearDamping = 0f,
            AngularDamping = 0f,
            GravityScale = 1f,
            Flags = FLAG_USE_GRAVITY
        };
    }

    public static Rigidbody2D Static()
    {
        return new Rigidbody2D
        {
            Mass = 0,
            InverseMass = 0,
            LinearDamping = 0,
            AngularDamping = 0,
            GravityScale = 0,
            Flags = 0
        };
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct PhysicsMaterial2D
{
    public float Friction;
    public float Restitution;
    public float PositionCorrectPercent;
}

[StructLayout(LayoutKind.Sequential)]
public struct BoxCollider2D
{
    public Vector2 HalfExtents;
    public Vector2 Offset;
    public float Rotation;
    public uint LayerMask;
    public uint Flags;

    public const uint FLAG_IS_TRIGGER = 1 << 0;

    public bool IsTrigger
    {
        readonly get => (Flags & FLAG_IS_TRIGGER) != 0;
        set => Flags = value ? (Flags | FLAG_IS_TRIGGER) : (Flags & ~FLAG_IS_TRIGGER);
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct CircleCollider2D
{
    public float Radius;
    public Vector2 Offset;
    public uint LayerMask;
    public uint Flags;

    public const uint FLAG_IS_TRIGGER = 1 << 0;

    public bool IsTrigger
    {
        readonly get => (Flags & FLAG_IS_TRIGGER) != 0;
        set => Flags = value ? (Flags | FLAG_IS_TRIGGER) : (Flags & ~FLAG_IS_TRIGGER);
    }

    public CircleCollider2D(float radius)
    {
        Radius = radius;
        Offset = Vector2.Zero;
        LayerMask = 0xFFFFFFFF;
        Flags = 0;
    }
}

// ---------------------------------- Rendering Components ----------------------------------

[StructLayout(LayoutKind.Sequential)]
public struct SpriteRenderer2D
{
    public uint TextureId;
    public Color Color;
    public Vector2 Tiling;
    public Vector2 Offset;
    public int Width;
    public int Height;
    public uint EmissiveTextureId;
    public float EmissiveStrength;
}

[StructLayout(LayoutKind.Sequential)]
public struct SpriteFlip2D
{
    public bool FlipX;
    public bool FlipY;
}

[StructLayout(LayoutKind.Sequential)]
public struct SpriteShader2D
{
    public bool Bloom;
}

[StructLayout(LayoutKind.Sequential)]
public struct SpriteSheetAnimation2D
{
    public uint TextureId;
    public int FrameWidth;
    public int FrameHeight;
    public int SheetWidth;
    public int SheetHeight;
    public int StartFrame;
    public int FrameCount;
    public float FramesPerSecond;
    public bool Loop;
    public bool Playing;
}

[StructLayout(LayoutKind.Sequential)]
public struct AnimationState2D
{
    public int CurrentFrame;
    public float TimeAccumulator;
    public bool Finished;
}

[StructLayout(LayoutKind.Sequential)]
public struct ShapeCircle2D
{
    public float Radius;
    public Vector2 Offset;
    public Color Color;
    public float Thickness;
    public bool Filled;
}

[StructLayout(LayoutKind.Sequential)]
public struct ShapeBox2D
{
    public Vector2 HalfExtents;
    public Vector2 Offset;
    public Color Color;
    public float Thickness;
    public bool Filled;
}

[StructLayout(LayoutKind.Sequential)]
public struct ShapeLine2D
{
    public Vector2 A;
    public Vector2 B;
    public Color Color;
    public float Thickness;
}

[StructLayout(LayoutKind.Sequential)]
public struct ZIndex2D
{
    public short ZOrder;
}

[StructLayout(LayoutKind.Sequential)]
public struct Light2D
{
    public enum Type : byte
    {
        Directional = 0,
        Point = 1
    }

    public Type LightType;
    public Vector3 Position;
    public Vector3 Direction;
    public Color Color;
    public float Intensity;
    public float Range;
    public bool CastsShadows;
}

public enum TextAnchor : byte
{
    Absolute = 0,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center
}

[StructLayout(LayoutKind.Sequential)]
public struct Text
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
    public char[] Content;
    
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
    public char[] FontPath;
    
    public float PixelSize;
    public Color Color;
    public TextAnchor Anchor;

    public Text(string content, float pixelSize = 24f, string fontPath = "assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf")
    {
        Content = new char[256];
        FontPath = new char[128];
        PixelSize = pixelSize;
        Color = new Color(1, 1, 1, 1);
        Anchor = TextAnchor.Absolute;

        if (!string.IsNullOrEmpty(content))
        {
            var chars = content.ToCharArray();
            Array.Copy(chars, Content, (int)GMath.Min(chars.Length, 255));
        }

        if (!string.IsNullOrEmpty(fontPath))
        {
            var chars = fontPath.ToCharArray();
            Array.Copy(chars, FontPath, (int)GMath.Min(chars.Length, 127));
        }
    }

    public readonly string GetContent() => new string(Content).TrimEnd('\0');
    public readonly string GetFontPath() => new string(FontPath).TrimEnd('\0');
}

[StructLayout(LayoutKind.Sequential)]
public struct UIButton
{
    public int ID;
    public float X, Y, W, H;
    public bool Hovered;
    public bool Pressed;
    public uint ActionID;
}

// ---------------------------------- Camera Components ----------------------------------

[StructLayout(LayoutKind.Sequential)]
public struct Camera3D
{
    public bool UsePerspective;
    public float FOV;
    public float NearPlane;
    public float FarPlane;
    public float OrthoSize;
    public float AspectRatio;
    public bool Active;

    public static Camera3D Orthographic(float size = 10f, float aspectRatio = 16f/9f)
    {
        return new Camera3D
        {
            UsePerspective = false,
            FOV = 45f,
            NearPlane = 0.1f,
            FarPlane = 100f,
            OrthoSize = size,
            AspectRatio = aspectRatio,
            Active = true
        };
    }

    public static Camera3D Perspective(float fov = 45f, float aspectRatio = 16f/9f)
    {
        return new Camera3D
        {
            UsePerspective = true,
            FOV = fov,
            NearPlane = 0.1f,
            FarPlane = 100f,
            OrthoSize = 10f,
            AspectRatio = aspectRatio,
            Active = true
        };
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct CameraMatrices
{
    public Matrix4x4 View;
    public Matrix4x4 Projection;
    public Matrix4x4 ViewProjection;
}

// ---------------------------------- Audio Components ----------------------------------

[StructLayout(LayoutKind.Sequential)]
public struct AudioSource
{
    public uint CueId;
    public float Volume;
    public float Pitch;
    public bool Loop;
    public bool PlayOnStart;
    public bool Spatial3D;

    public AudioSource(uint cueId)
    {
        CueId = cueId;
        Volume = 1.0f;
        Pitch = 1.0f;
        Loop = false;
        PlayOnStart = false;
        Spatial3D = true;
    }
}

// ---------------------------------- Additional Types ----------------------------------

[StructLayout(LayoutKind.Sequential)]
public struct Layer
{
    public ushort Id;
}

[StructLayout(LayoutKind.Sequential)]
public struct Color
{
    public float R, G, B, A;

    public Color(float r, float g, float b, float a = 1f)
    {
        R = r;
        G = g;
        B = b;
        A = a;
    }

    public static Color White => new(1, 1, 1, 1);
    public static Color Black => new(0, 0, 0, 1);
    public static Color Red => new(1, 0, 0, 1);
    public static Color Green => new(0, 1, 0, 1);
    public static Color Blue => new(0, 0, 1, 1);
    public static Color Yellow => new(1, 1, 0, 1);
    public static Color Cyan => new(0, 1, 1, 1);
    public static Color Magenta => new(1, 0, 1, 1);
}

[StructLayout(LayoutKind.Sequential)]
public struct Matrix4x4
{
    // Row-major 4x4 matrix
    public float M11, M12, M13, M14;
    public float M21, M22, M23, M24;
    public float M31, M32, M33, M34;
    public float M41, M42, M43, M44;
}

/// <summary>
/// Component type registry using compile-time hash matching.
/// Type hashes MUST match the C++ side component type IDs.
/// </summary>
internal static class ComponentTypeRegistry
{
    // FNV-1a hash function - matches C++ ComponentType::Hash()
    private static uint FNV1aHash(string str)
    {
        // FNV-1a prime and offset
        const uint fnvPrime = 0x01000193;
        const uint fnvOffset = 0x811C9DC5;

        var hash = fnvOffset;
        foreach (var c in str)
        {
            hash ^= c;
            hash *= fnvPrime;
        }

        return hash;
    }

    /// <summary>
    /// Get the type hash for a component type.
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <returns></returns>
    public static uint GetTypeHash<T>() where T : unmanaged
    {
        var name = typeof(T).Name;
        var hash = FNV1aHash(name);

        // Uncomment this for debugging
        // Log the computed type name and hash to help debug mismatches with the native side
        // Logging.Log($"ComponentTypeRegistry: Type '{name}' -> Hash {hash}", LogLevel.Debug);

        // Uncomment this and replacement the if statement to debug unexpected primitive types
        // So if an unexpected primitive type like Boolean is being used as a component type,
        // log a stacktrace so we can find the call-site that requested it.
        // if (name == "Boolean")
        // {
        //     try
        //     {
        //         var st = new StackTrace(1, true).ToString();
        //         Logging.Log($"ComponentTypeRegistry: {name} requested from:\n{st}", LogLevel.Error);
        //     }
        //     catch (Exception ex)
        //     {
        //         // If stacktrace capture fails, log a warning (new StackTrace())
        //         Logging.Log($"ComponentTypeRegistry: Failed to capture stacktrace: {ex.Message}", LogLevel.Warning);
        //     }
        // }
        return hash;
    }
}
