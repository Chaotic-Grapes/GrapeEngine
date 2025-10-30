using System;
using System.Collections.Generic;
using GrapeEngine.Scripting;
using GrapeEngine.Numerics;

namespace MyGame
{
    // base state class 

    // abstract class for all states in the HFSM
    public abstract class State
    {
        // parent state
        public State Parent { get; set; }

        // child state
        public List<State> Children { get; set; } = new List<State>();

        // currently active child states if any
        public State ActiveChild { get; set; }

        //called once if any enter this state
        public abstract void OnEnter();


        // called every frame while state 
        public abstract void OnUpdate(float deltaTime);

        // called once when exitting state
        public abstract void OnExit();

        // checks for transitions to other states 
        // returns the target state if a transition is triggered,else null
        public abstract State CheckTransitions();
    }


    //HFSM MANAGER
    //Manages state transitions and executes the current state's logic

    public class HFSM
    {
        // the state currently active
        public State CurrentState { get; private set; }

        // root top level state
        public State RootState { get; private set; }

        // to track state history
        public Stack<State> StateHistory { get; private set; } = new Stack<State>();

        //init with a start state 
        public void Initialize(State rootState)
        {
            RootState = rootState;
            TransitionTo(rootState);
        }

        // update the HFSM each frames
        public void Update(float deltaTime)
        {
            if (CurrentState == null)
                return;

            // Update current state
            CurrentState.OnUpdate(deltaTime);

            // Check for transitions
            State newState = CheckHierarchyTransitions(CurrentState);

            if (newState != null && newState != CurrentState)
            {
                TransitionTo(newState);
            }
        }

        // check transitions up the hierarchy 
        private State CheckHierarchyTransitions(State fromState)
        {
            // Check current state first
            State targetState = fromState.CheckTransitions();
            if (targetState != null)
                return targetState;

            // Check parent states recursively
            if (fromState.Parent != null)
                return CheckHierarchyTransitions(fromState.Parent);

            return null;
        }

        //transition from current to new state 
        public void TransitionTo(State newState)
        {
            if (CurrentState == newState)
                return;

            // Exit current state
            if (CurrentState != null)
            {
                CurrentState.OnExit();
            }

            // Enter new state
            CurrentState = newState;
            CurrentState.OnEnter();
            StateHistory.Push(newState);
        }
    }
}