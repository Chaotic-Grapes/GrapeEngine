/* Start Header *****************************************************************/
/*!
\file    Interop_StateMachine.cpp
\author  Dalton Koh 
\par     d.koh@digipen.edu
\brief
Interop bindings for the C++ Brain/StateMachine used by gameplay scripts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "services/StateMachine.h"

using Engine::Gameplay::Brain;
using Engine::Gameplay::State;

// Create a new Brain and return as opaque pointer.
INTEROP_API void* StateMachineInterop_CreateBrain() {
    return new Brain();
}

// Destroy a Brain created by the interop layer.
INTEROP_API void StateMachineInterop_DestroyBrain(void* brainPtr) {
    if (!brainPtr) {
        return;
    }
    delete static_cast<Brain*>(brainPtr);
}

// Update the Brain state machine.
INTEROP_API void StateMachineInterop_Update(void* brainPtr, float deltaTime) {
    if (!brainPtr) {
        return;
    }
    static_cast<Brain*>(brainPtr)->Update(deltaTime);
}

// Transition the Brain to a new state.
INTEROP_API void StateMachineInterop_TransitionTo(void* brainPtr, void* statePtr) {
    if (!brainPtr) {
        return;
    }
    static_cast<Brain*>(brainPtr)->TransitionTo(static_cast<State*>(statePtr));
}

// Return the current state pointer.
INTEROP_API void* StateMachineInterop_GetCurrentState(void* brainPtr) {
    if (!brainPtr) {
        return nullptr;
    }
    return static_cast<void*>(static_cast<Brain*>(brainPtr)->GetCurrentState());
}

// Return patrol state pointer.
INTEROP_API void* StateMachineInterop_GetPatrolState(void* brainPtr) {
    if (!brainPtr) {
        return nullptr;
    }
    return static_cast<void*>(static_cast<Brain*>(brainPtr)->GetPatrolState());
}

// Return chase state pointer.
INTEROP_API void* StateMachineInterop_GetChaseState(void* brainPtr) {
    if (!brainPtr) {
        return nullptr;
    }
    return static_cast<void*>(static_cast<Brain*>(brainPtr)->GetChaseState());
}

// Return attack state pointer.
INTEROP_API void* StateMachineInterop_GetAttackState(void* brainPtr) {
    if (!brainPtr) {
        return nullptr;
    }
    return static_cast<void*>(static_cast<Brain*>(brainPtr)->GetAttackState());
}
