#ifndef MESSAGEBUS_H
#define MESSAGEBUS_H

#include <vector>
#include <functional>

namespace Messaging {
    class MessageBus {
    public:
        template<typename T>
        using Handler = std::function<void(const T&)>;

        // Subscribe to a message type
        template<typename T>
        static void Subscribe(Handler<T> handler) {
            auto& subscribers = GetHandlers<T>();
            subscribers.push_back(handler);
        }

        // Broadcast a message to all subscribers
        template<typename T>
        static void Broadcast(const T& message) {
            auto& subscribers = GetHandlers<T>();
            for (auto& handler : subscribers) {
                handler(message);
            }
        }

        // Clear all subscriptions for type T
        template<typename T>
        static void Clear() {
            GetHandlers<T>().clear();
        }

    private:
        // Each message type has its own static storage
        template<typename T>
        static std::vector<Handler<T>>& GetHandlers() {
            static std::vector<Handler<T>> handlers;
            return handlers;
        }
    };
}

#endif // MESSAGEBUS_H


