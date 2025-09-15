#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "IComponent.h"

namespace Component {
    // TODO: Replace with a math library vector type
    struct Transform : IComponent {
        float X = 0, Y = 0;
        float Rotation = 0;
		float ScaleX = 1, ScaleY = 1;
    };
}

#endif
