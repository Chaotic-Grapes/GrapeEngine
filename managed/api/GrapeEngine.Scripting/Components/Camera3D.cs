using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct Camera3D
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
