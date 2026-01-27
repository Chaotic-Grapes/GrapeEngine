#include "services/StateMachine.h"


/**
 ~Summary~
  C++ Brain (This file):
 - Owns all states (PatrolState, ChaseState, AttackState)
 - Tracks the current state
 - Provides methods to:
    - GetCurrentState() - Check which state we're in
    - GetPatrolState() / GetChaseState() / GetAttackState() - Get state references
    - TransitionTo(newState) - Perform transitions
    - Update(deltaTime) - Update state lifecycle
 
 C# EnemyAI (Not in this file):
  - Decides WHEN to transition
  - Checks distances and conditions
  - Calls brain.TransitionTo() to make transitions
  - Handles all game logic (damage, movement, animations)
  
  Data Flow:
  1. C# checks: distance < 50 && current == PatrolState
  2. C# calls: brain.TransitionTo(brain.GetChaseState())
  3. C++ executes: Calls ExitAction(), switches state, calls EntryAction()
  4. Next frame: C# checks conditions again and may transition
*/

namespace Engine {
    namespace Gameplay {


		// Basically use the states below to track and swap in
		// This logic container setup

        class PatrolState : public State
        {
        public:
			// Logic in C#
        };

        class ChaseState : public State
        {
        public:
			// Logic in C#
        };

        class AttackState : public State
        {
        public:
			// Logic in C#
        };


        // BRAIN IMPLEMENTATION
        // The Brain class is the main AI controller.
        // It owns the FSM (state machine) and all the states.
        // C# interacts ONLY with the Brain, not directly with FSM or states.

        Brain::Brain()
        {
            // Allocate memory for each predefined state
            m_patrolState = new PatrolState();
            m_chaseState = new ChaseState();
            m_attackState = new AttackState();

            // Start in patrol state by default
            m_currentState = m_patrolState;

        }

		// Dtor
        Brain::~Brain()
        {
            // Free memory - delete all states
            delete m_patrolState;
            delete m_chaseState;
            delete m_attackState;

            // Clear pointers 
            m_patrolState = nullptr;
            m_chaseState = nullptr;
            m_attackState = nullptr;
            m_currentState = nullptr;

        }

        
          //TransitionTo - Core transition function
          //What it does:
          // 1. Validates the new state is valid and different from current
          // 2. Calls ExitAction() on the current state (cleanup)
          // 3. Updates m_currentState to the new state
          // 4. Calls EntryAction() on the new state (setup)
         
        void Brain::TransitionTo(State* newState)
        {
            // Safety check -> Don't transition if newState is null
            if (!newState)
                return;

            // Safety check -> Don't transition to the same state
            if (newState == m_currentState)
                return;

			// Clear currentState if state exists
            if (m_currentState)
            {
                m_currentState->OnExit();
            }

			// Then switch to newState
            m_currentState = newState;

			// Set current to initialize funct
            m_currentState->OnEnter();

        }

		// Ret CurrentState
        State* Brain::GetCurrentState() const
        {
            return m_currentState;
        }


		// Below are functs to get the 3 diff states that 
		// are called by C# code to get references of current 
		// state
        State* Brain::GetPatrolState() const
        {
            return m_patrolState;
        }

        State* Brain::GetChaseState() const
        {
            return m_chaseState;
        }

        State* Brain::GetAttackState() const
        {
            return m_attackState;
        }


        void Brain::Update(float deltaTime)
        {
            // Safety check -> only update if we have a current state
            if (!m_currentState)
                return;

            m_currentState->OnUpdate(deltaTime);

        }

    } 
}