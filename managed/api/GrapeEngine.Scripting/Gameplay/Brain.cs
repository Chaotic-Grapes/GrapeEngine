/* Start Header *****************************************************************/
/*!
\file   Brain.cs
\author Dalton Koh 
\brief
Managed wrapper for the native Brain state machine.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Gameplay;

public sealed class Brain : IDisposable
{
    // Native handle to the Brain instance.
    public nint Handle { get; }
    // Tracks whether this wrapper owns the native handle.
    private readonly bool _ownsHandle;

    // Private constructor used by factory methods.
    private Brain(nint handle, bool ownsHandle)
    {
        Handle = handle;
        _ownsHandle = ownsHandle;
    }

    // Create a new native Brain instance.
    public static Brain Create()
    {
        nint handle = StateMachineAPI.CreateBrain();
        if (handle == nint.Zero)
        {
            throw new InvalidOperationException("Failed to create Brain.");
        }
        return new Brain(handle, true);
    }

    // Wrap an existing native handle.
    public static Brain FromHandle(nint handle)
    {
        if (handle == nint.Zero)
        {
            throw new ArgumentException("Brain handle is null.", nameof(handle));
        }
        return new Brain(handle, false);
    }

    // Update the native Brain.
    public void Update(float deltaTime)
    {
        StateMachineAPI.Update(Handle, deltaTime);
    }

    // Transition the native Brain to a new state.
    public void TransitionTo(nint statePtr)
    {
        StateMachineAPI.TransitionTo(Handle, statePtr);
    }

    // Get the current state pointer.
    public nint GetCurrentState()
    {
        return StateMachineAPI.GetCurrentState(Handle);
    }

    // Get the patrol state pointer.
    public nint GetPatrolState()
    {
        return StateMachineAPI.GetPatrolState(Handle);
    }

    // Get the chase state pointer.
    public nint GetChaseState()
    {
        return StateMachineAPI.GetChaseState(Handle);
    }

    // Get the attack state pointer.
    public nint GetAttackState()
    {
        return StateMachineAPI.GetAttackState(Handle);
    }

    // Dispose the native Brain if owned.
    public void Dispose()
    {
        if (_ownsHandle && Handle != nint.Zero)
        {
            StateMachineAPI.DestroyBrain(Handle);
        }
    }

    // Destroy a native Brain handle directly.
    public static void DestroyHandle(nint handle)
    {
        if (handle != nint.Zero)
        {
            StateMachineAPI.DestroyBrain(handle);
        }
    }
}
