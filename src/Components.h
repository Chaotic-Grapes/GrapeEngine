#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "IComponent.h"

namespace Component {
    struct Transform : IComponent {
        float X = 0, Y = 0;
        float Rotation = 0;
		float ScaleX = 1, ScaleY = 1;

        std::unique_ptr<IComponent> Clone() const override {
            return std::make_unique<Transform>(*this);
        }
    };
}

#endif
