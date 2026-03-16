namespace GrapeEngine.Scripting.Systems.Attributes;

/// <summary>
/// System execution mode - determines when systems are active.
/// MUST match C++ ECS::SystemRunMode enum values for correct P/Invoke marshaling.
/// </summary>
public enum SystemRunMode
{
    Always = 0,
    PlayOnly = 1,
    EditOnly = 2
}
