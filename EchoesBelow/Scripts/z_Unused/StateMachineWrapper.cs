/**
* These classes are wrappers that allow C# to interact with the C++ Brain
* and State system. The actual logic containers are in C++, but C# can
* create and control them through these wrappers.
*/

using System;
using System.Collections.Generic;

namespace MyGame
{
    // C# WRAPPER: State base class
    // This is a C# representation of the C++ State class.
    // C++ will call these virtual methods on C# state instances.
    public abstract class State
    {
        // Called once when entering this state
        // Use this to initialize state-specific setup
        public virtual void OnEnter()
        {
        }
        // Called every frame while in this state
        // Use this for per-frame state logic
        public virtual void OnUpdate(float deltaTime)
        {
        }
        // Called once when exiting this state
        // Use this for cleanup
        public virtual void OnExit()
        {
        }
    }

    // C# WRAPPER: Brain class
    // Brain class - C++ logic container wrapper
    // 
    // The Brain owns the FSM and all predefined states (Patrol, Chase, Attack).
    // This is a wrapper that C# can instantiate to control the C++ FSM.
    // Responsibilities:
    // - Own and manage the C++ FSM
    // - Provide access to predefined states
    // - Handle transitions when C# tells it to

    /// Usage from C#:
    /// 1. Create: m_brain = new Brain();
    /// 2. Get states: State patrol = m_brain.GetPatrolState();
    /// 3. Transition: m_brain.TransitionTo(chaseState);
    /// 4. Query: State current = m_brain.GetCurrentState();

    public class Brain
    {

        // Brain Constructor
        // Creates a new Brain instance.
        // The C++ side handles the actual construction and initialization.
        public Brain()
        {
            // C++ constructor is called automatically by the engine
        }


        // TransitionTo - Tell C++ to transition to a new state
        public void TransitionTo(State newState)
        {
            // C++ implementation handles the actual transition
            // This method is called by C# to request a state change
        }

        // GetCurrentState - Query which state we're in
        public State? GetCurrentState()
        {
            // C++ implementation returns the current state
            return null;  // Will be overridden by C++ binding
        }


        // GetPatrolState - Get reference to Patrol state
        public State? GetPatrolState()
        {
            // C++ implementation returns the patrol state
            return null;  // Will be overridden by C++ binding
        }


        // GetChaseState - Get reference to Chase state
        public State? GetChaseState()
        {
            // C++ implementation returns the chase state
            return null;  // Will be overridden by C++ binding
        }

        // GetAttackState - Get reference to Attack state
        public State? GetAttackState()
        {
            // C++ implementation returns the attack state
            return null;  // Will be overridden by C++ binding
        }

        // Update - Update the Brain each frame
        public void Update(float deltaTime)
        {
            // C++ implementation handles the FSM update
        }
    }
}