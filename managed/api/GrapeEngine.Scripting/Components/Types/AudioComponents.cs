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

namespace GrapeEngine.Scripting;

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
