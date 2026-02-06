/* Start Header *****************************************************************/
/*!
\file    Interop_StateMachine.cpp
\author
\brief
Interop bindings for the C++ Brain/StateMachine used by gameplay scripts.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "services/StateMachine.h"

using Engine::Gameplay::Brain;
using Engine::Gameplay::State;

INTEROP_API void* StateMachineInterop_CreateBrain() {
    return new Brain();
}

INTEROP_API void StateMachineInterop_DestroyBrain(void* brainPtr) {
    if (!brainPtr) {
        return;
    }
    delete static_cast<Brain*>(brainPtr);
}

INTEROP_API void StateMachineInterop_Update(void* brainPtr, float deltaTime) {
    if (!brainPtr) {
        return;
    }
    static_cast<Brain*>(brainPtr)->Update(deltaTime);
}

INTEROP_API void StateMachineInterop_TransitionTo(void* brainPtr, void* statePtr) {
    if (!brainPtr) {
        return;
    }
    static_cast<Brain*>(brainPtr)->TransitionTo(static_cast<State*>(statePtr));
}

INTEROP_API void* StateMachineInterop_GetCurrentState(void* brainPtr) {
    if (!brainPtr) {
        return nullptr;
    }
    return static_cast<void*>(static_cast<Brain*>(brainPtr)->GetCurrentState());
}

INTEROP_API void* StateMachineInterop_GetPatrolState(void* brainPtr) {
    if (!brainPtr) {
        return nullptr;
    }
    return static_cast<void*>(static_cast<Brain*>(brainPtr)->GetPatrolState());
}

INTEROP_API void* StateMachineInterop_GetChaseState(void* brainPtr) {
    if (!brainPtr) {
        return nullptr;
    }
    return static_cast<void*>(static_cast<Brain*>(brainPtr)->GetChaseState());
}

INTEROP_API void* StateMachineInterop_GetAttackState(void* brainPtr) {
    if (!brainPtr) {
        return nullptr;
    }
    return static_cast<void*>(static_cast<Brain*>(brainPtr)->GetAttackState());
}
