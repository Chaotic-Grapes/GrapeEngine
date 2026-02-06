using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Gameplay;

public sealed class Brain : IDisposable
{
    public nint Handle { get; }
    private readonly bool _ownsHandle;

    private Brain(nint handle, bool ownsHandle)
    {
        Handle = handle;
        _ownsHandle = ownsHandle;
    }

    public static Brain Create()
    {
        nint handle = StateMachineAPI.CreateBrain();
        if (handle == nint.Zero)
        {
            throw new InvalidOperationException("Failed to create Brain.");
        }
        return new Brain(handle, true);
    }

    public static Brain FromHandle(nint handle)
    {
        if (handle == nint.Zero)
        {
            throw new ArgumentException("Brain handle is null.", nameof(handle));
        }
        return new Brain(handle, false);
    }

    public void Update(float deltaTime)
    {
        StateMachineAPI.Update(Handle, deltaTime);
    }

    public void TransitionTo(nint statePtr)
    {
        StateMachineAPI.TransitionTo(Handle, statePtr);
    }

    public nint GetCurrentState()
    {
        return StateMachineAPI.GetCurrentState(Handle);
    }

    public nint GetPatrolState()
    {
        return StateMachineAPI.GetPatrolState(Handle);
    }

    public nint GetChaseState()
    {
        return StateMachineAPI.GetChaseState(Handle);
    }

    public nint GetAttackState()
    {
        return StateMachineAPI.GetAttackState(Handle);
    }

    public void Dispose()
    {
        if (_ownsHandle && Handle != nint.Zero)
        {
            StateMachineAPI.DestroyBrain(Handle);
        }
    }

    public static void DestroyHandle(nint handle)
    {
        if (handle != nint.Zero)
        {
            StateMachineAPI.DestroyBrain(handle);
        }
    }
}
