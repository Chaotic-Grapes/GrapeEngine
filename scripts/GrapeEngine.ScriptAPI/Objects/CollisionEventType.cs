namespace GrapeEngine.Scripting;

/// <summary>
/// Mirror of native ECS::CollisionEventType for script code.
/// </summary>
public enum CollisionEventType : int
{
    Enter = 0,
    Stay = 1,
    Exit = 2
}
