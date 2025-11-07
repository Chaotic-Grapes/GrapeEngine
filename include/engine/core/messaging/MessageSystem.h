/*****************************************************************************/
/*!
\file       MessageSystem.h
\author     Samantha Leong (s.leong@digipen.edu)
\par        DigiPen login: 2403088
\date       2025-11-03
\brief
 * Implements a generic and type-safe publish–subscribe (observer) messaging system.
 * Systems or game objects can subscribe to messages of specific types and
 * be notified when messages are broadcast.
 *
 * The system supports:
 * - Priority-based message delivery (higher priority observers receive first)
 * - Optional message filtering
 * - Scoped subscriptions (auto-unsubscribe when out of scope)
 * - Enabling/disabling subscriptions without removal
 *
 * @date 2025-11-03
 * @copyright
 * DigiPen Institute of Technology. All rights reserved.
 *
 * @section usage How To Use
 *
 * @code{.cpp}
 * // Example: Define a message struct
 * struct CollisionEvent {
 *     EntityId entityA;
 *     EntityId entityB;
 * };
 *
 * // Example: Subscribing to a message in another system (e.g., PhysicsSystem.cpp)
 * auto handle = Messaging::MessageSystem::Subscribe<CollisionEvent>(
 *     [](const CollisionEvent& e) {
 *         std::cout << "Collision detected between: " << e.entityA << " and " << e.entityB << std::endl;
 *     },
 *     1 // priority (optional)
 * );
 *
 * // Example: Sending a message
 * CollisionEvent evt{playerId, wallId};
 * Messaging::MessageSystem::Notify(evt);
 *
 * // Example: Unsubscribing
 * Messaging::MessageSystem::Unsubscribe<CollisionEvent>(handle);
 *
 * // Example: Scoped subscription (auto unsubscribes on destruction)
 * Messaging::ScopedSubscription<CollisionEvent> scopedSub(
 *     Messaging::MessageSystem::Subscribe<CollisionEvent>(
 *         [](const CollisionEvent& e) { // handle event here }
 *     )
 * );
 * @endcode
 *
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without prior
written consent of DigiPen Institute of Technology is prohibited.
/*****************************************************************************/

#ifndef MESSAGEBUS_H
#define MESSAGEBUS_H

#include <vector>
#include <functional>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <string>

#include "MessageTypes.h" 

namespace Messaging {
    // Forward declaration
    template<typename T>
    class Observable;

    /**
    * @brief
    * Handle used to manage a subscription, allowing for unsubscription or disabling.
    */
    // Subscription handle for managing subscriptions
    class SubscriptionHandle {
    public:
        SubscriptionHandle() : id_(0), valid_(false) {}
        SubscriptionHandle(size_t id) : id_(id), valid_(true) {}

        /// @return The unique ID associated with this subscription.
        size_t GetId() const { return id_; }
        /// @return True if the handle currently represents a valid subscription.
        bool IsValid() const { return valid_; }
        /// @brief Invalidates the handle (usually after unsubscription).
        void Invalidate() { valid_ = false; }

    private:
        size_t id_;
        bool valid_;
    };

    /**
    * @brief
    * Interface for an observer listening to messages of type T.
    * Can be subclassed or wrapped in a FunctionObserver.
    */
    // Observer interface - follows Observer pattern
    template<typename T>
    class IObserver {
    public:
        virtual ~IObserver() = default;
        /// @brief Called when a message of type T is received.
        virtual void OnNotify(const T& message) = 0;
        /// @return The priority of this observer (higher = earlier notification).
        virtual int GetPriority() const { return 0; }
        /// @brief Allows selective reception based on message content.
        virtual bool ShouldReceive(const T& message) const { return true; }
    };

     /**
     * @brief
     * A wrapper class for lambda or std::function observers.
     */
    // Concrete observer wrapper for lambda/function handlers
    template<typename T>
    class FunctionObserver : public IObserver<T> {
    public:
        using Handler = std::function<void(const T&)>;
        using Filter = std::function<bool(const T&)>;
        /**
        * @param handler The function to invoke on message receipt.
        * @param priority The observer’s priority (higher = notified first).
        * @param filter Optional lambda that returns true if this observer should receive a given message.
        */
        FunctionObserver(Handler handler, int priority = 0, Filter filter = nullptr)
            : handler_(handler), priority_(priority), filter_(filter) {
        }

        void OnNotify(const T& message) override {
            handler_(message);
        }

        int GetPriority() const override { return priority_; }

        bool ShouldReceive(const T& message) const override {
            return filter_ ? filter_(message) : true;
        }

    private:
        Handler handler_;
        int priority_;
        Filter filter_;
    };

    /**
    * @brief
    Observable class that manages a list of observers and notifies them.
    */
    // Observable - manages observers and notifications
    template<typename T>
    class Observable {
    public:
        struct ObserverEntry {
            size_t id;
            std::shared_ptr<IObserver<T>> observer;
            bool enabled;

            ObserverEntry(size_t i, std::shared_ptr<IObserver<T>> obs)
                : id(i), observer(obs), enabled(true) {
            }
        };

        /**
         * @brief Subscribes a new observer to messages of type T.
         * @param handler The callback function to invoke.
         * @param priority Observer priority (higher = first).
         * @param filter Optional filter predicate to control which messages are received.
         * @return A SubscriptionHandle representing this observer.
         */
        // Subscribe with priority and optional filter
        SubscriptionHandle Subscribe(
            std::function<void(const T&)> handler,
            int priority = 0,
            std::function<bool(const T&)> filter = nullptr
        ) {
            auto observer = std::make_shared<FunctionObserver<T>>(handler, priority, filter);
            size_t id = next_id_++;

            observers_.emplace_back(id, observer);

            // Sort by priority (higher priority first)
            std::sort(observers_.begin(), observers_.end(),
                [](const ObserverEntry& a, const ObserverEntry& b) {
                    return a.observer->GetPriority() > b.observer->GetPriority();
                });

            return SubscriptionHandle(id);
        }

        /**
         * @brief Unsubscribes an observer using its handle.
         * @param handle Reference to the handle (invalidated after removal).
         */
        // Unsubscribe using handle
        void Unsubscribe(SubscriptionHandle& handle) {
            if (!handle.IsValid()) return;

            auto it = std::find_if(observers_.begin(), observers_.end(),
                [&handle](const ObserverEntry& entry) {
                    return entry.id == handle.GetId();
                });

            if (it != observers_.end()) {
                observers_.erase(it);
                handle.Invalidate();
            }
        }

        /**
         * @brief Enables or disables a specific subscription without removing it.
         * @param handle The subscription handle.
         * @param enabled True to enable, false to disable.
         */
        // Temporarily disable/enable a subscription
        void SetEnabled(const SubscriptionHandle& handle, bool enabled) {
            auto it = std::find_if(observers_.begin(), observers_.end(),
                [&handle](const ObserverEntry& entry) {
                    return entry.id == handle.GetId();
                });

            if (it != observers_.end()) {
                it->enabled = enabled;
            }
        }

        /**
        * @brief Notifies all enabled observers of a message.
        * @param message The message to broadcast.
        */
        // Notify all observers
        void Notify(const T& message) {
            // Copy vector to allow modifications during iteration
            // Make a copy to allow modifications (e.g., unsubscribe) during iteration
            auto observers_copy = observers_;

            for (auto& entry : observers_copy) {
                if (entry.enabled && entry.observer->ShouldReceive(message)) {
                    entry.observer->OnNotify(message);
                }
            }
        }

        /// @return The number of currently enabled observers.
        // Get number of active observers
        size_t GetObserverCount() const {
            return std::count_if(observers_.begin(), observers_.end(),
                [](const ObserverEntry& entry) { return entry.enabled; });
        }

        /// @brief Clears all observers from this observable.
        // Clear all observers
        void Clear() {
            observers_.clear();
        }

    private:
        std::vector<ObserverEntry> observers_;
        size_t next_id_ = 1;
    };

    /**
     * @brief
     * Global messaging system managing all message types.
     * Provides static methods to subscribe, unsubscribe, and broadcast messages.
     */
    // Global MessageSystem using Observer pattern
    class MessageSystem {
    public:
        /**
         * @brief Subscribes to messages of type T.
         * @param handler Function called when a message of type T is sent.
         * @param priority Determines order of notification.
         * @param filter Optional filter to selectively receive messages.
         * @return A SubscriptionHandle to manage the subscription.
         */
        // Subscribe to a message type with priority and filter
        template<typename T>
        static SubscriptionHandle Subscribe(
            std::function<void(const T&)> handler,
            int priority = 0,
            std::function<bool(const T&)> filter = nullptr
        ) {
            return GetObservable<T>().Subscribe(handler, priority, filter);
        }

        /// @brief Unsubscribes a previously registered observer.
        // Unsubscribe using handle
        template<typename T>
        static void Unsubscribe(SubscriptionHandle& handle) {
            GetObservable<T>().Unsubscribe(handle);
        }

        /// @brief Enables or disables an existing subscription.
        // Enable/disable subscription
        template<typename T>
        static void SetEnabled(const SubscriptionHandle& handle, bool enabled) {
            GetObservable<T>().SetEnabled(handle, enabled);
        }

        /// @brief Broadcasts a message to all subscribed observers.
        // Notify all observers (formerly Broadcast)
        template<typename T>
        static void Notify(const T& message) {
            GetObservable<T>().Notify(message);
        }

        /// @brief Legacy alias for Notify()
        // Legacy support - Broadcast calls Notify
        template<typename T>
        static void Broadcast(const T& message) {
            Notify(message);
        }

        /// @brief Provides direct access to the Observable for advanced control.
        // Get observable for direct manipulation
        template<typename T>
        static Observable<T>& GetObservable() {
            static Observable<T> observable;
            return observable;
        }

        /// @brief Clears all subscriptions for message type T.
        // Clear all subscriptions for type T
        template<typename T>
        static void Clear() {
            GetObservable<T>().Clear();
        }

        /// @return The number of active observers for type T.
        // Get observer count
        template<typename T>
        static size_t GetObserverCount() {
            return GetObservable<T>().GetObserverCount();
        }
    };

    /**
     * @brief
     * RAII wrapper for automatic unsubscription upon destruction.
     */
    // RAII wrapper for automatic unsubscription
    template<typename T>
    class ScopedSubscription {
    public:
        ScopedSubscription(SubscriptionHandle handle)
            : handle_(handle) {
        }

        ~ScopedSubscription() {
            MessageSystem::Unsubscribe<T>(handle_);
        }

        // Non-copyable
        ScopedSubscription(const ScopedSubscription&) = delete;
        ScopedSubscription& operator=(const ScopedSubscription&) = delete;

        // Movable
        ScopedSubscription(ScopedSubscription&& other) noexcept
            : handle_(other.handle_) {
            other.handle_.Invalidate();
        }
        
        /// @return Reference to the underlying handle.
        const SubscriptionHandle& GetHandle() const { return handle_; }

    private:
        SubscriptionHandle handle_;
    };
    
    //class MessageSystem {
    //public:
    //    template<typename T>
    //    using Handler = std::function<void(const T&)>;

    //    // Subscribe to a message type
    //    template<typename T>
    //    static void Subscribe(Handler<T> handler) {
    //        auto& subscribers = GetHandlers<T>();
    //        subscribers.push_back(handler);
    //    }

    //    // Broadcast a message to all subscribers
    //    template<typename T>
    //    static void Broadcast(const T& message) {
    //        auto& subscribers = GetHandlers<T>();
    //        for (auto& handler : subscribers) {
    //            handler(message);
    //        }
    //    }

    //    // Clear all subscriptions for type T
    //    template<typename T>
    //    static void Clear() {
    //        GetHandlers<T>().clear();
    //    }

    //private:
    //    // Each message type has its own static storage
    //    template<typename T>
    //    static std::vector<Handler<T>>& GetHandlers() {
    //        static std::vector<Handler<T>> handlers;
    //        return handlers;
    //    }
    //};
}

#endif // MESSAGEBUS_H


