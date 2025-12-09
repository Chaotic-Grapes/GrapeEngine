/* Start Header *****************************************************************/
/*!
\file   CameraComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Camera-related ECS component types.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Components.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting;

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

[StructLayout(LayoutKind.Sequential)]
public record struct CameraMatrices
{
    public Matrix4x4 View;
    public Matrix4x4 Projection;
    public Matrix4x4 ViewProjection;
}
