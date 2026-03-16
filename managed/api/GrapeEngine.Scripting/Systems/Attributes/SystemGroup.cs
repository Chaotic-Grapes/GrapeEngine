namespace GrapeEngine.Scripting.Systems.Attributes;

/// <summary>
/// System execution group - defines when systems run relative to engine lifecycle.
/// MUST match C++ ECS::SystemGroup enum values for correct P/Invoke marshaling.
/// </summary>
public enum SystemGroup
{
    PreUpdate = 0,
    Update = 1,
    PostUpdate = 2,
    PrePhysics = 3,
    Physics = 4,
    PostPhysics = 5,
    PreRender = 6,
    Render = 7,
    PostRender = 8
}
