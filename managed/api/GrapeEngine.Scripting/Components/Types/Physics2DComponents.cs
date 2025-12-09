/* Start Header *****************************************************************/
/*!
\file   Physics2DComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
2D physics component types.
*/
/* End Header *******************************************************************/

using GrapeEngine.Numerics;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting;

[StructLayout(LayoutKind.Sequential)]
public record struct LinearVelocity2D
{
    public Vector2 Value;

    public LinearVelocity2D(Vector2 value) => Value = value;
    public LinearVelocity2D(float x, float y) => Value = new Vector2(x, y);
}

[StructLayout(LayoutKind.Sequential)]
public record struct Acceleration2D
{
    public Vector2 Value;

    public Acceleration2D(Vector2 value) => Value = value;
    public Acceleration2D(float x, float y) => Value = new Vector2(x, y);
}

[StructLayout(LayoutKind.Sequential)]
public record struct AngularVelocity2D
{
    public float Value;

    public AngularVelocity2D(float value) => Value = value;
}

[StructLayout(LayoutKind.Sequential)]
public record struct Rigidbody2D
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
public record struct PhysicsMaterial2D
{
    public float Friction;
    public float Restitution;
    public float PositionCorrectPercent;
}

[StructLayout(LayoutKind.Sequential)]
public record struct BoxCollider2D
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
public record struct CircleCollider2D
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
