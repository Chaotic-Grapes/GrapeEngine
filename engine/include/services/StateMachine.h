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
		// Destructor 
		virtual ~Transition() = default;

		// Get target state to transition to 
		virtual State* GetTargetState() = 0;

		// Evaluate if condition is met and ret true if transition should 
		// Happen, false otherwise.
		virtual bool Condition() = 0;
	};

	// State class to represent states in FSM
	class State {
    public:

		// Dtor
        virtual ~State() = default;

		// On enter/exit/update functionalities
        virtual void OnEnter() {}
        virtual void OnUpdate(float deltaTime) {}
        virtual void OnExit() {}
    };

	// Class for FSM 
	// List states, active state, update in a loop
	class FiniteStateMachine {
	public:
		// Default ctr
		FiniteStateMachine();
		// Dtor
		~FiniteStateMachine();

		// Function runners 
		void Initialize(/*States root state*/);
		void Update(/*States current state*/);
		void TransitionTo(/*States new state*/);

		// Get Specific states 
		State* GetCurrentState() const;
		State* GetIdleState() const;
		State* GetChaseState() const;
		State* GetAttackState() const;

	private:
		
		// predefined states we can use to cordinate with
		State* m_idleState();
		State* m_attackState();
		State* m_chaseState();
		State* m_currentState();
		
		// Transition to a new state
		void TransitionTo(State* newState);
	};

	// Image this as a top layer wrapper that will function as the 
	// main brain that controls each state change for each AI-entity
	class Brain {
    public:
        Brain();
        ~Brain();

        // C# calls this to make transitions
        void TransitionTo(State* newState);

        // C# queries this to check current state
        State* GetCurrentState() const;

        // Get specific states
        State* GetPatrolState() const;
        State* GetChaseState() const;
        State* GetAttackState() const;

        // update for cycle for states
        void Update(float deltaTime);

    private:
		// Running States
        State* m_patrolState;
        State* m_chaseState;
        State* m_attackState;
        State* m_currentState;
    };
} // namespace Engine::Gameplay