#ifndef ICOMPONENT_H
#define ICOMPONENT_H

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Component {
    struct IComponent {
        virtual ~IComponent() = default;

        virtual const char* GetTypeName() const = 0;
    };
}

#endif
