/* Start Header *****************************************************************/
/*!
\file   StateMachine.h
\author Dalton Koh (100%)
\par    d.koh@digipen.edu
\brief
Declares the Transition, State, FiniteStateMachine, and Brain classes used
to implement finite state machine behaviour for AI-controlled entities.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/


#pragma once
#include "core/Application.h"
#include "time.h"
#include <vector>
#include <memory>


namespace Engine::Gameplay {

	// forward declarations for other classess
	class State;
	// class Brain;
	// class FiniteStateMachine;
		
	// Transition class to help with the engagement of C# and C++
	// Communication and container state transitions
	class Transition {
	public:
		virtual ~Transition() = default;

		/**
		 * @brief Get the target state this transition leads to.
		 * @return Pointer to the target State.
		 */
		virtual State* GetTargetState() = 0;

		/**
		 * @brief Evaluate whether the transition condition is satisfied.
		 * @return True if the transition should fire, false otherwise.
		 */
		virtual bool Condition() = 0;
	};

	// State class to represent states in FSM
	class State {
    public:
        virtual ~State() = default;

        /** @brief Called when the FSM enters this state. */
        virtual void OnEnter() {}

        /**
         * @brief Called each frame while this state is active.
         * @param deltaTime Time elapsed since the last frame in seconds.
         */
        virtual void OnUpdate(float /*deltaTime*/) {}

        /** @brief Called when the FSM exits this state. */
        virtual void OnExit() {}
    };

	// Class for FSM
	// List states, active state, update in a loop
	class FiniteStateMachine {
	public:
		FiniteStateMachine();
		~FiniteStateMachine();

		/** @brief Set the initial state and prepare the FSM for running. */
		void Initialize(/*States root state*/);

		/** @brief Evaluate transitions and invoke the current state's update. */
		void Update(/*States current state*/);

		/**
		 * @brief Transition to a new state by pointer.
		 * @param newState State to transition to.
		 */
		void TransitionTo(/*States new state*/);

		/**
		 * @brief Get the currently active state.
		 * @return Pointer to the current State.
		 */
		State* GetCurrentState() const;

		/**
		 * @brief Get the pre-defined idle state.
		 * @return Pointer to the idle State.
		 */
		State* GetIdleState() const;

		/**
		 * @brief Get the pre-defined chase state.
		 * @return Pointer to the chase State.
		 */
		State* GetChaseState() const;

		/**
		 * @brief Get the pre-defined attack state.
		 * @return Pointer to the attack State.
		 */
		State* GetAttackState() const;

	private:
		State* m_idleState();
		State* m_attackState();
		State* m_chaseState();
		State* m_currentState();

		/**
		 * @brief Internal transition: exit the current state and enter the new one.
		 * @param newState State to activate.
		 */
		void TransitionTo(State* newState);
	};

	// Top-level wrapper controlling AI-entity state changes.
	class Brain {
    public:
        Brain();
        ~Brain();

        /**
         * @brief Transition the AI to a new state (callable from C#).
         * @param newState State to activate.
         */
        void TransitionTo(State* newState);

        /**
         * @brief Get the currently active AI state (queryable from C#).
         * @return Pointer to the current State.
         */
        State* GetCurrentState() const;

        /**
         * @brief Get the pre-defined patrol state.
         * @return Pointer to the patrol State.
         */
        State* GetPatrolState() const;

        /**
         * @brief Get the pre-defined chase state.
         * @return Pointer to the chase State.
         */
        State* GetChaseState() const;

        /**
         * @brief Get the pre-defined attack state.
         * @return Pointer to the attack State.
         */
        State* GetAttackState() const;

        /**
         * @brief Advance the current state's update logic.
         * @param deltaTime Time elapsed since the last frame in seconds.
         */
        void Update(float deltaTime);

    private:
		// Running States
        State* m_patrolState;
        State* m_chaseState;
        State* m_attackState;
        State* m_currentState;
    };
} // namespace Engine::Gameplay