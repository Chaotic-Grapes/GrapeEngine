using GrapeEngine.Numerics;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting
{
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
            Array.Copy(chars, Value, Math.Min(chars.Length, 63));
        }

        public override string ToString() => new string(Value).TrimEnd('\0');
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
    public struct Lifetime
    {
        public float Time;
    }

    // ---------------------------------- Transform Components ----------------------------------

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

        public static LocalTransform Default => new LocalTransform(Vector3.Zero, Quaternion.Identity, Vector3.One);
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
            get => (Flags & FLAG_KINEMATIC) != 0;
            set => Flags = value ? (Flags | FLAG_KINEMATIC) : (Flags & ~FLAG_KINEMATIC);
        }

        public bool UseGravity
        {
            get => (Flags & FLAG_USE_GRAVITY) != 0;
            set => Flags = value ? (Flags | FLAG_USE_GRAVITY) : (Flags & ~FLAG_USE_GRAVITY);
        }

        public bool FixedRotation
        {
            get => (Flags & FLAG_FIXED_ROTATION) != 0;
            set => Flags = value ? (Flags | FLAG_FIXED_ROTATION) : (Flags & ~FLAG_FIXED_ROTATION);
        }

        public static Rigidbody2D Dynamic(float mass = 1.0f)
        {
            return new Rigidbody2D
            {
                Mass = mass,
                InverseMass = mass > 0 ? 1.0f / mass : 0,
                LinearDamping = 0.0f,
                AngularDamping = 0.0f,
                GravityScale = 1.0f,
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
            get => (Flags & FLAG_IS_TRIGGER) != 0;
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
            get => (Flags & FLAG_IS_TRIGGER) != 0;
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

    // ---------------------------------- 3D Physics Components ----------------------------------

    [StructLayout(LayoutKind.Sequential)]
    public struct Velocity
    {
        public Vector3 Value;

        public Velocity(Vector3 value) => Value = value;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Acceleration
    {
        public Vector3 Value;

        public Acceleration(Vector3 value) => Value = value;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct AngularVelocity
    {
        public Vector3 Value;

        public AngularVelocity(Vector3 value) => Value = value;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Rigidbody
    {
        public float Mass;
        public float InverseMass;
        public float LinearDrag;
        public float AngularDrag;
        public uint Flags;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct BoxCollider
    {
        public Vector3 HalfExtents;
        public uint LayerMask;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SphereCollider
    {
        public float Radius;
        public uint LayerMask;
    }

    // ---------------------------------- Rendering Components ----------------------------------

    [StructLayout(LayoutKind.Sequential)]
    public struct SpriteRenderer2D
    {
        public uint TextureId;
        public Color Color;
        public Vector2 Tiling;
        public Vector2 Offset;
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

    // ---------------------------------- Camera Components ----------------------------------

    [StructLayout(LayoutKind.Sequential)]
    public struct Camera
    {
        public bool IsOrthographic;
        public float FovY;
        public float OrthoHeight;
        public float Near;
        public float Far;
        public float Aspect;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct CameraMatrices
    {
        public Matrix4x4 View;
        public Matrix4x4 Projection;
        public Matrix4x4 ViewProjection;
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

        public Color(float r, float g, float b, float a = 1.0f)
        {
            R = r; G = g; B = b; A = a;
        }

        public static Color White => new Color(1, 1, 1, 1);
        public static Color Black => new Color(0, 0, 0, 1);
        public static Color Red => new Color(1, 0, 0, 1);
        public static Color Green => new Color(0, 1, 0, 1);
        public static Color Blue => new Color(0, 0, 1, 1);
        public static Color Yellow => new Color(1, 1, 0, 1);
        public static Color Cyan => new Color(0, 1, 1, 1);
        public static Color Magenta => new Color(1, 0, 1, 1);
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
            return FNV1aHash(typeof(T).Name);
        }
    }
}
