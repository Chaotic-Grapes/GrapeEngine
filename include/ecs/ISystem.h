#ifndef ISYSTEM_H
#define ISYSTEM_H

#include <string>

namespace Engine {
    class ISystem {
public:
		/**
		 * @brief Called when the system is being destroyed/cleaned up
		 */
        virtual ~ISystem() = default;

        /**
		 * @brief Called when the system is created/initialized
         */
        virtual void OnCreate() = 0;

        /**
		 * @brief Called every frame to update the system's state
         */
        virtual void OnUpdate() = 0;

        /**
		 * @brief Called every frame after OnUpdate for any late updates
         */
		virtual void OnLateUpdate() {}

        /**
		 * @brief Name of the system for debugging/logging purposes
		 * 
		 * @return std::string Name of the system
         */
        virtual std::string Name() const = 0;
};
}
#endif // ISYSTEM_H
