#ifndef MATHHELPER_H
#define MATHHELPER_H

#include <random>

class MathUtils {
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

    template <typename T>
    static T Clamp(T value, T minVal, T maxVal) {
        return value < minVal 
            ? minVal 
            : (value > maxVal ? maxVal : value);
    }
};
#endif