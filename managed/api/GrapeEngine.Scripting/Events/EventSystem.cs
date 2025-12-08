using System;
using System.Collections.Generic;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Events;
{
    /// <summary>
    /// Manages event component lifecycle and provides utilities for working with events.
    /// This system is responsible for tracking event components and clearing them at appropriate times.
    /// </summary>
    public class EventSystem
    {
        private World world;
        private HashSet<ulong> activeCollisionEvents = new HashSet<ulong>();
        private HashSet<ulong> activeTriggerEvents = new HashSet<ulong>();
        private HashSet<ulong> activeCollisionExitEvents = new HashSet<ulong>();
        private HashSet<ulong> activeTriggerExitEvents = new HashSet<ulong>();

        /// <summary>
        /// Initialize the event system with a world reference.
        /// </summary>
        public EventSystem(World world)
        {
            this.world = world ?? throw new ArgumentNullException(nameof(world));
        }

        /// <summary>
        /// Track a collision event that occurred this frame.
        /// Typically called by the physics system.
        /// </summary>
        public void TrackCollisionEvent(ulong entityId)
        {
            activeCollisionEvents.Add(entityId);
        }

        /// <summary>
        /// Track a trigger event that occurred this frame.
        /// Typically called by the physics system.
        /// </summary>
        public void TrackTriggerEvent(ulong entityId)
        {
            activeTriggerEvents.Add(entityId);
        }

        /// <summary>
        /// Track a collision exit event that occurred this frame.
        /// Typically called by the physics system.
        /// </summary>
        public void TrackCollisionExitEvent(ulong entityId)
        {
            activeCollisionExitEvents.Add(entityId);
        }

        /// <summary>
        /// Track a trigger exit event that occurred this frame.
        /// Typically called by the physics system.
        /// </summary>
        public void TrackTriggerExitEvent(ulong entityId)
        {
            activeTriggerExitEvents.Add(entityId);
        }

        /// <summary>
        /// Check if an entity has a collision event this frame.
        /// </summary>
        public bool HasCollisionEvent(ulong entityId)
        {
            return activeCollisionEvents.Contains(entityId);
        }

        /// <summary>
        /// Check if an entity has a trigger event this frame.
        /// </summary>
        public bool HasTriggerEvent(ulong entityId)
        {
            return activeTriggerEvents.Contains(entityId);
        }

        /// <summary>
        /// Check if an entity has a collision exit event this frame.
        /// </summary>
        public bool HasCollisionExitEvent(ulong entityId)
        {
            return activeCollisionExitEvents.Contains(entityId);
        }

        /// <summary>
        /// Check if an entity has a trigger exit event this frame.
        /// </summary>
        public bool HasTriggerExitEvent(ulong entityId)
        {
            return activeTriggerExitEvents.Contains(entityId);
        }

        /// <summary>
        /// Called at the end of each frame to clear all event components.
        /// This ensures events only persist for a single frame.
        /// </summary>
        public void ClearFrameEvents()
        {
            // Clear collision events
            foreach (ulong entityId in activeCollisionEvents)
            {
                var entity = Entity.FromId(world, entityId);
                if (entity.IsAlive && entity.HasComponent<CollisionEvent>())
                {
                    entity.RemoveComponent<CollisionEvent>();
                }
            }
            activeCollisionEvents.Clear();

            // Clear trigger events
            foreach (ulong entityId in activeTriggerEvents)
            {
                var entity = Entity.FromId(world, entityId);
                if (entity.IsAlive && entity.HasComponent<TriggerEvent>())
                {
                    entity.RemoveComponent<TriggerEvent>();
                }
            }
            activeTriggerEvents.Clear();

            // Clear collision exit events
            foreach (ulong entityId in activeCollisionExitEvents)
            {
                var entity = Entity.FromId(world, entityId);
                if (entity.IsAlive && entity.HasComponent<CollisionExitEvent>())
                {
                    entity.RemoveComponent<CollisionExitEvent>();
                }
            }
            activeCollisionExitEvents.Clear();

            // Clear trigger exit events
            foreach (ulong entityId in activeTriggerExitEvents)
            {
                var entity = Entity.FromId(world, entityId);
                if (entity.IsAlive && entity.HasComponent<TriggerExitEvent>())
                {
                    entity.RemoveComponent<TriggerExitEvent>();
                }
            }
            activeTriggerExitEvents.Clear();
        }

        /// <summary>
        /// Get the count of active collision events this frame.
        /// </summary>
        public int CollisionEventCount => activeCollisionEvents.Count;

        /// <summary>
        /// Get the count of active trigger events this frame.
        /// </summary>
        public int TriggerEventCount => activeTriggerEvents.Count;

        /// <summary>
        /// Get the count of active collision exit events this frame.
        /// </summary>
        public int CollisionExitEventCount => activeCollisionExitEvents.Count;

        /// <summary>
        /// Get the count of active trigger exit events this frame.
        /// </summary>
        public int TriggerExitEventCount => activeTriggerExitEvents.Count;
    }
}
