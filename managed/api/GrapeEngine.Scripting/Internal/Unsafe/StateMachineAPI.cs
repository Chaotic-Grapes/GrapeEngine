using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

internal static partial class StateMachineAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_CreateBrain")]
    internal static partial nint CreateBrain();

    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_DestroyBrain")]
    internal static partial void DestroyBrain(nint brainPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_Update")]
    internal static partial void Update(nint brainPtr, float deltaTime);

    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_TransitionTo")]
    internal static partial void TransitionTo(nint brainPtr, nint statePtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_GetCurrentState")]
    internal static partial nint GetCurrentState(nint brainPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_GetPatrolState")]
    internal static partial nint GetPatrolState(nint brainPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_GetChaseState")]
    internal static partial nint GetChaseState(nint brainPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "StateMachineInterop_GetAttackState")]
    internal static partial nint GetAttackState(nint brainPtr);
}
