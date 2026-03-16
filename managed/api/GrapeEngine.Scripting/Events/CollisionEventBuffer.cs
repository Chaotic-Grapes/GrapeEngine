using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Events;


/// <summary>
/// Buffer of collision events for a single entity in the current frame.
/// </summary>
public struct CollisionEventBuffer
{
    public const int MaxEvents = 8;
    public int Count;
    public CollisionEvent Event0;
    public CollisionEvent Event1;
    public CollisionEvent Event2;
    public CollisionEvent Event3;
    public CollisionEvent Event4;
    public CollisionEvent Event5;
    public CollisionEvent Event6;
    public CollisionEvent Event7;

    public CollisionEvent GetEvent(int index)
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
