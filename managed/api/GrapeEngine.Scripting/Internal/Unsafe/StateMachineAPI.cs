/* Start Header *****************************************************************/
/*!
\file   StateMachineAPI.cs
\author Dalton Koh 
\brief
P/Invoke declarations for the native state machine interop API.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

internal static partial class StateMachineAPI
{
    // Create a new native Brain instance.
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_CreateBrain")]
    internal static partial nint CreateBrain();

    // Destroy a native Brain instance.
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_DestroyBrain")]
    internal static partial void DestroyBrain(nint brainPtr);

    // Update Brain with delta time.
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_Update")]
    internal static partial void Update(nint brainPtr, float deltaTime);

    // Transition Brain to a new state.
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_TransitionTo")]
    internal static partial void TransitionTo(nint brainPtr, nint statePtr);

    // Get current state pointer.
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_GetCurrentState")]
    internal static partial nint GetCurrentState(nint brainPtr);

    // Get patrol state pointer.
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_GetPatrolState")]
    internal static partial nint GetPatrolState(nint brainPtr);

    // Get chase state pointer.
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_GetChaseState")]
    internal static partial nint GetChaseState(nint brainPtr);

    // Get attack state pointer.
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_GetAttackState")]
    internal static partial nint GetAttackState(nint brainPtr);
}
