using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct BoidFlock
{
    public int Count;
    public float SeparationWeight;
    public float AlignmentWeight;
    public float CohesionWeight;
    public float CollisionAvoidWeight;
    public float CollisionAvoidRadius;
    public float VisualRange;
    public float MaxSpeed;
    public float MaxForce;
    public float BoidSize;
    public uint TextureId;
    public StringId TexturePath;
}
