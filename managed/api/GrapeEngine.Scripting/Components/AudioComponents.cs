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
using GrapeEngine.Scripting.Services;

namespace GrapeEngine.Scripting.Components;

[StructLayout(LayoutKind.Sequential)]
public record struct AudioSource(uint CueId)
{
    // Basics
    public uint CueId = CueId;
    public float Volume = 1.0f;
    public float Pitch = 1.0f;
    public bool Loop = false;
    public bool PlayOnStart = false;
    public bool Spatial3D = true;
    public AudioBus Bus = AudioBus.SFX;
    public float Pan = 0.0f;

    // Fade flags
    public bool EnableFadeIn = false;
    public bool EnableFadeOut = false;

    // Fade durations (seconds)
    public float FadeInDuration = 1.0f;
    public float FadeOutDuration = 1.0f;
}

