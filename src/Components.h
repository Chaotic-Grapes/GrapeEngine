#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "IComponent.h"

namespace Component {
    // TODO: Replace with a math library vector type
    struct Transform : IComponent {
        float X = 0, Y = 0;
        float Rotation = 0;
		float ScaleX = 1, ScaleY = 1;

        explicit Transform(const float x = 0, const float y = 0, const float rotation = 0, const float scaleX = 1.f, const float scaleY = 1.f)
			: X(x), Y(y), Rotation(rotation), ScaleX(scaleX), ScaleY(scaleY) { }
    };
}

#endif
