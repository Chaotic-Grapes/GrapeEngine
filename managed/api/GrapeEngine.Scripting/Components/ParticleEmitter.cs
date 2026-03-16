using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct ParticleEmitter
{
    public uint PresetId;
    public int MaxParticles;
    public float EmissionRate;
    public int BurstCount;
    public float ParticleSize;
    public bool Active;

    public uint TextureId;
    public StringId TexturePath;

    public float SpeedMin;
    public float SpeedMax;
    public float GravityX;
    public float GravityY;
    public float Drag;
    public float Turbulence;
    public float WobbleFrequency;
    public float WobbleAmplitude;
    public float SizeStart;
    public float SizeEnd;
    public float LifetimeMin;
    public float LifetimeMax;
    public float EmissionAngle;
    public float EmissionSpread;
    public float EmissionRadius;
    public byte EmissionShape;
    public float ColorStartR;
    public float ColorStartG;
    public float ColorStartB;
    public float ColorStartA;
    public float ColorEndR;
    public float ColorEndG;
    public float ColorEndB;
    public float ColorEndA;
    public bool DieOnCollision;
    public float Bounciness;
    public bool KillOutOfBounds;
    public float RotationSpeedMin;
    public float RotationSpeedMax;
}
