/* Start Header *****************************************************************/
/*!
\file   MessageSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli
\par    muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of a simple MessageBus system for decoupled
communication between components.

Usage:
- Subscribe to messages of type T using Subscribe<T>(handler).
- Broadcast messages of type T using Broadcast<T>(message).
- Clear all subscriptions for type T using Clear<T>().

Example:
\code
// Subscribe to int messages
Messaging::MessageBus::Subscribe<int>([](const int& msg) {
    std::cout << "Received int message: " << msg << '\n';
});

// Broadcast an int message
Messaging::MessageBus::Broadcast<int>(42);
\endcode

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef MESSAGEBUS_H
#define MESSAGEBUS_H

#include <vector>
#include <functional>

namespace Messaging {
    class MessageSystem {
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


