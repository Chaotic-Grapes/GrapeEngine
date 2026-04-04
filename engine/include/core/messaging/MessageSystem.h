/* Start Header *****************************************************************/
/*!
\file   MessageSystem.h
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   3rd November 2025
\brief
Implements a generic and type-safe publish-subscribe (observer) messaging system.
Supports:
- Priority-based message delivery (higher priority observers receive first)
- Optional message filtering
- Scoped subscriptions (auto-unsubscribe when out of scope)
- Enabling/disabling subscriptions without removal

@section usage How To Use
@code{.cpp}
// Example: Define a message struct
struct CollisionEvent {
    EntityId entityA;
    EntityId entityB;
};

// Example: Subscribing to a message in another system (e.g., PhysicsSystem.cpp)
auto handle = Messaging::MessageSystem::Subscribe<CollisionEvent>(
    [](const CollisionEvent& e) {
        std::cout << "Collision detected between: " << e.entityA << " and " << e.entityB << std::endl;
    },
    1 // priority (optional)
);

// Example: Sending a message
CollisionEvent evt{playerId, wallId};
Messaging::MessageSystem::Notify(evt);

// Example: Unsubscribing
Messaging::MessageSystem::Unsubscribe<CollisionEvent>(handle);

// Example: Scoped subscription (auto unsubscribes on destruction)
Messaging::ScopedSubscription<CollisionEvent> scopedSub(
    Messaging::MessageSystem::Subscribe<CollisionEvent>(
        [](const CollisionEvent& e) {
            // handle event here
        }
    )
);
@endcode
*/
/* End Header *******************************************************************/

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

    // Handle used to manage a subscription, allowing for unsubscription or disabling.
    // Subscription handle for managing subscriptions
    class SubscriptionHandle {
    public:
        SubscriptionHandle() : id_(0), valid_(false) {}
        SubscriptionHandle(size_t id) : id_(id), valid_(true) {}

        /**
         * @brief Get the unique ID associated with this subscription.
         * @return Unique subscription identifier.
         */
        size_t GetId() const { return id_; }

        /**
         * @brief Check whether the handle currently represents a valid subscription.
         * @return True if the handle has not been invalidated.
         */
        bool IsValid() const { return valid_; }

        /**
         * @brief Invalidate the handle (called after unsubscription).
         */
        void Invalidate() { valid_ = false; }

    private:
        size_t id_;
        bool valid_;
    };

    // Observer interface for messages of type T; can use FunctionObserver.
    // Observer interface - follows Observer pattern
    template<typename T>
    class IObserver {
    public:
        virtual ~IObserver() = default;
        /**
         * @brief Handle an incoming message of type T.
         * @param message Message to process.
         */
        virtual void OnNotify(const T& message) = 0;

        /**
         * @brief Get the observer's notification priority.
         * @return Priority value; higher values are notified first.
         */
        virtual int GetPriority() const { return 0; }

        /**
         * @brief Check whether this observer should receive the given message.
         * @param message Message to evaluate.
         * @return True if the observer wants to receive this message.
         */
        virtual bool ShouldReceive(const T& message) const { (void)message; return true; }
    };

    // Wrapper for lambda or std::function observers.
    // Concrete observer wrapper for lambda/function handlers
    template<typename T>
    class FunctionObserver : public IObserver<T> {
    public:
        using Handler = std::function<void(const T&)>;
        using Filter = std::function<bool(const T&)>;
        /**
         * @brief Construct with a handler function; higher priority is notified first.
         * @param handler Callable invoked when a matching message is received.
         * @param priority Notification priority; higher values fire first.
         * @param filter Optional predicate; observer only receives messages that pass.
         */
        FunctionObserver(Handler handler, int priority = 0, Filter filter = nullptr)
            : handler_(handler), priority_(priority), filter_(filter) {
        }

        void OnNotify(const T& message) override {
            if (handler_) {
                handler_(message);
            }
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

    // Observable class that manages a list of observers and notifies them.
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
         * @brief Subscribe a handler function; higher priority observers are notified first.
         * @param handler Callable invoked when a matching message is broadcast.
         * @param priority Notification priority; higher values fire first.
         * @param filter Optional predicate; handler only fires for messages that pass.
         * @return SubscriptionHandle that can be used to unsubscribe or toggle the subscription.
         */
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
         * @brief Unsubscribe an observer; the handle is invalidated after removal.
         * @param handle Handle of the subscription to remove.
         */
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
         * @brief Enable or disable a subscription without removing it.
         * @param handle Handle of the subscription to toggle.
         * @param enabled True to enable the subscription, false to suppress it.
         */
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
         * @brief Notify all enabled observers of a message.
         * @param message Message to broadcast to all subscribed observers.
         */
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

        /**
         * @brief Get the number of currently enabled observers.
         * @return Count of enabled observer entries.
         */
        size_t GetObserverCount() const {
            return std::count_if(observers_.begin(), observers_.end(),
                [](const ObserverEntry& entry) { return entry.enabled; });
        }

        /** @brief Clear all observers from this observable. */
        void Clear() {
            observers_.clear();
        }

    private:
        std::vector<ObserverEntry> observers_;
        size_t next_id_ = 1;
    };

    // Global messaging system managing all message types.
    // Global MessageSystem using Observer pattern
    class MessageSystem {
    public:
        /**
         * @brief Subscribe to messages of type T with optional priority and filter.
         * @tparam T Message type to subscribe to.
         * @param handler Callable invoked when a message of type T is broadcast.
         * @param priority Notification priority; higher values fire first.
         * @param filter Optional predicate; handler only fires for messages that pass.
         * @return SubscriptionHandle for managing the subscription lifetime.
         */
        template<typename T>
        static SubscriptionHandle Subscribe(
            std::function<void(const T&)> handler,
            int priority = 0,
            std::function<bool(const T&)> filter = nullptr
        ) {
            return GetObservable<T>().Subscribe(handler, priority, filter);
        }

        /**
         * @brief Unsubscribe a previously registered observer.
         * @tparam T Message type the handle was subscribed to.
         * @param handle Handle to unsubscribe (invalidated after removal).
         */
        template<typename T>
        static void Unsubscribe(SubscriptionHandle& handle) {
            GetObservable<T>().Unsubscribe(handle);
        }

        /**
         * @brief Enable or disable an existing subscription without removing it.
         * @tparam T Message type the handle was subscribed to.
         * @param handle Handle of the subscription to toggle.
         * @param enabled True to enable, false to suppress.
         */
        template<typename T>
        static void SetEnabled(const SubscriptionHandle& handle, bool enabled) {
            GetObservable<T>().SetEnabled(handle, enabled);
        }

        /**
         * @brief Broadcast a message to all subscribed observers of type T.
         * @tparam T Message type to broadcast.
         * @param message Message instance to deliver.
         */
        template<typename T>
        static void Notify(const T& message) {
            GetObservable<T>().Notify(message);
        }

        /**
         * @brief Legacy alias for Notify().
         * @tparam T Message type to broadcast.
         * @param message Message instance to deliver.
         */
        template<typename T>
        static void Broadcast(const T& message) {
            Notify(message);
        }

        /**
         * @brief Get direct access to the Observable for type T for advanced control.
         * @tparam T Message type whose Observable to retrieve.
         * @return Reference to the static Observable<T> instance.
         */
        template<typename T>
        static Observable<T>& GetObservable() {
            static Observable<T> observable;
            return observable;
        }

        /**
         * @brief Clear all subscriptions for message type T.
         * @tparam T Message type whose subscriptions to clear.
         */
        template<typename T>
        static void Clear() {
            GetObservable<T>().Clear();
        }

        /**
         * @brief Get the number of active observers for message type T.
         * @tparam T Message type to query.
         * @return Count of enabled observers for T.
         */
        template<typename T>
        static size_t GetObserverCount() {
            return GetObservable<T>().GetObserverCount();
        }
    };

    /**
     * @brief Scoped RAII handle that unsubscribes automatically on destruction.
     * RAII wrapper for automatic unsubscription upon destruction.
     */
    template<typename T>
    class ScopedSubscription {
    public:
        /**
         * @brief Take ownership of a subscription handle for RAII-managed lifetime.
         * @param handle Subscription handle to manage.
         */
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
        
        /**
         * @brief Get a reference to the underlying subscription handle.
         * @return Reference to the managed SubscriptionHandle.
         */
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


