/* Start Header *****************************************************************/
/*!
\file   AudioComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Audio-related ECS component types.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;

[Component]
[StructLayout(LayoutKind.Sequential)]
public record struct AudioSource(uint CueId)
{
    public uint CueId = CueId;
    public float Volume = 1.0f;
    public float Pitch = 1.0f;
    public bool Loop = false;
    public bool PlayOnStart = false;
    public bool Spatial3D = true;
}

