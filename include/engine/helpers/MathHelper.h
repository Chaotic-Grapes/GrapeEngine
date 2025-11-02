#ifndef MATHHELPER_H
#define MATHHELPER_H

#include <random>
#include <cmath>

class MathHelper {
public:
    // Randomize function template
    template <typename T>
    static T Randomize(T minVal, T maxVal, const unsigned seed = 0) {
        // Seed
        thread_local std::mt19937 engine;
        if (seed != 0)
            engine.seed(seed);
        else {
            // Default seed otherwise
            thread_local std::random_device rd;
            engine.seed(rd());
        }

        if constexpr (std::is_integral_v<T>) {
            std::uniform_int_distribution<T> dist(minVal, maxVal);
            return dist(engine);
        }
        else if constexpr (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> dist(minVal, maxVal);
            return dist(engine);
        }
		else return 0; // Unsupported type
    }

    // Distance function
    static float Distance(const Vector2D& a, const Vector2D& b) {
        float dx = a.X - b.X;
        float dy = a.Y - b.Y;
        return std::sqrt(dx * dx + dy * dy);
    }
};
#endif