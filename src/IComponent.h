#ifndef ICOMPONENT_H
#define ICOMPONENT_H

#include <memory>
#include <typeindex>

namespace Component {
    struct IComponent {
        virtual ~IComponent() = default;
        virtual std::unique_ptr<IComponent> Clone() const = 0;
    };
}

#endif
