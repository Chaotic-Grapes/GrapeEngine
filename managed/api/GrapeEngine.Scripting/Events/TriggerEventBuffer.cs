using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Events;


/// <summary>
/// Buffer of trigger events for a single entity in the current frame.
/// </summary>
public struct TriggerEventBuffer
{
    public const int MaxEvents = 8;
    public int Count;
    public TriggerEvent Event0;
    public TriggerEvent Event1;
    public TriggerEvent Event2;
    public TriggerEvent Event3;
    public TriggerEvent Event4;
    public TriggerEvent Event5;
    public TriggerEvent Event6;
    public TriggerEvent Event7;

    public TriggerEvent GetEvent(int index)
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
