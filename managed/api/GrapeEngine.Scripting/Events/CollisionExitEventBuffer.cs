using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Events;


/// <summary>
/// Buffer of collision exit events for a single entity in the current frame.
/// </summary>
public struct CollisionExitEventBuffer
{
    public const int MaxEvents = 8;
    public int Count;
    public CollisionExitEvent Event0;
    public CollisionExitEvent Event1;
    public CollisionExitEvent Event2;
    public CollisionExitEvent Event3;
    public CollisionExitEvent Event4;
    public CollisionExitEvent Event5;
    public CollisionExitEvent Event6;
    public CollisionExitEvent Event7;

    public CollisionExitEvent GetEvent(int index)
    {
        return index switch
        {
            0 => Event0,
            1 => Event1,
            2 => Event2,
            3 => Event3,
            4 => Event4,
            5 => Event5,
            6 => Event6,
            7 => Event7,
            _ => default
        };
    }
}
