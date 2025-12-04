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
        explicit IService(std::string name) : m_name(std::move(name)), m_enabled(true) {}
        virtual ~IService() = default;

        // Lifecycle hooks
        virtual void Initialize() {}
        virtual void Update() {}
        virtual void Terminate() {}

        // For services that need rendering last
        virtual void Render() {}

        // Enable/disable the system
        void SetEnabled(const bool e) { m_enabled = e; }
        bool IsEnabled() const { return m_enabled; }

        // Introspection
        virtual std::string Name() const { return m_name; }

    protected:
        // Simple log helper available to derived systems
        static void Trace(const std::string& msg);

    private:
        std::string m_name;
        bool        m_enabled;
    };
}

#endif 
