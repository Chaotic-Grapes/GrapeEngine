using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct Rigidbody2D
{
    public float Mass;
    private float _inverseMass;
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

    public readonly float InverseMass => _inverseMass;

    public static Rigidbody2D Dynamic(float mass = 1.0f)
    {
        return new Rigidbody2D
        {
            Mass = mass,
            _inverseMass = mass > 0 ? 1f / mass : 0,
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
            _inverseMass = 0,
            LinearDamping = 0,
            AngularDamping = 0,
            GravityScale = 0,
            Flags = 0
        };
    }
}
