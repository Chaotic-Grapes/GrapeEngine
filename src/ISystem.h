#ifndef ISYSTEM_H
#define ISYSTEM_H

#include <string>

namespace Engine {
    class ISystem {
public:
        virtual ~ISystem() = default;
    
        /// Called when attaching to the engine
        virtual void Initialize() = 0;

        /// Called every frame.
        virtual void Update() = 0;

        /// A string name for debugging
        virtual std::string Name() const = 0;
};
}
#endif // ISYSTEM_H
