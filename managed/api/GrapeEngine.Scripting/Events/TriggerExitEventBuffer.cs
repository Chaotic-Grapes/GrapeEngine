using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Events;


/// <summary>
/// Buffer of trigger exit events for a single entity in the current frame.
/// </summary>
public struct TriggerExitEventBuffer
{
    public const int MaxEvents = 8;
    public int Count;
    public TriggerExitEvent Event0;
    public TriggerExitEvent Event1;
    public TriggerExitEvent Event2;
    public TriggerExitEvent Event3;
    public TriggerExitEvent Event4;
    public TriggerExitEvent Event5;
    public TriggerExitEvent Event6;
    public TriggerExitEvent Event7;

    public TriggerExitEvent GetEvent(int index)
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
