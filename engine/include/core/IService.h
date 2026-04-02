/* Start Header *****************************************************************/
/*!
\file   IService.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   5th October 2025
\brief
Declares the IService interface for engine services.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ISERVICE_H
#define ISERVICE_H

#include <string>

namespace Engine {
    class IService {
    public:
        /**
         * @brief Construct service base with display name.
         * @param name Service name used for diagnostics/logging.
         */
        explicit IService(std::string name) : m_name(std::move(name)), m_enabled(true) {}

        /**
         * @brief Virtual destructor for polymorphic service cleanup.
         */
        virtual ~IService() = default;

        // Lifecycle hooks
        /**
         * @brief Initialize service resources before updates begin.
         */
        virtual void Initialize() {}

        /**
         * @brief Perform per-frame service update.
         */
        virtual void Update() {}

        /**
         * @brief Release service resources during shutdown.
         */
        virtual void Terminate() {}

        // For services that need rendering last
        /**
         * @brief Optional render hook for services with draw work.
         */
        virtual void Render() {}

        // Enable/disable the system
        /**
         * @brief Enable or disable this service.
         * @param e True to enable update/render hooks.
         */
        void SetEnabled(const bool e) { m_enabled = e; }

        /**
         * @brief Query whether this service is currently enabled.
         * @return True when service is enabled.
         */
        bool IsEnabled() const { return m_enabled; }

        // Introspection
        /**
         * @brief Get user-facing service name.
         * @return Service name string.
         */
        virtual std::string Name() const { return m_name; }

    protected:
        // Simple log helper available to derived systems
        /**
         * @brief Emit trace log message tagged for services.
         * @param msg Message text to log.
         */
        static void Trace(const std::string& msg);

    private:
        std::string m_name;
        bool        m_enabled;
    };
}

#endif 
