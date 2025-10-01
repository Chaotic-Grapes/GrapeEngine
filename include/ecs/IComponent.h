#ifndef ICOMPONENT_H
#define ICOMPONENT_H

#include "../include/nlohmann/json.hpp"

using json = nlohmann::json;

namespace Component {
    struct IComponent {
        virtual ~IComponent() = default;

        virtual json Serialize() const = 0;
        virtual void Deserialize(const json& data) = 0;
        virtual const char* GetTypeName() const = 0;
    };
}

#endif
