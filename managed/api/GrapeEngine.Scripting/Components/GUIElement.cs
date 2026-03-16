using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
    public record struct GUIElement
    {
        public Vector2 Position;
        public Vector2 Size;
        public bool Visible;
        public GUIAlignment Alignment;
        public short ZOrder;
        public Vector4 Margin;
        public Vector4 Padding;
        public Vector2 AnchorMin;
        public Vector2 AnchorMax;
        public Vector2 ResolvedPosition;
        public Vector2 ResolvedSize;
        public Vector2 ContentPosition;
        public Vector2 ContentSize;
        public Vector2 ScreenPosition;
        public Vector2 ScreenSize;
    }
