
#include "System.h"
#include <iostream>

namespace Systems {

    System::System(std::string name)
        : m_name(std::move(name)), m_enabled(true) {
    }

    System::~System() = default;

    void System::Initialize() {
        // default no-op
    }

    void System::Update(float /*dt*/) {
        // default no-op
    }

    void System::Terminate() {
        // default no-op
    }

    void System::SetEnabled(bool e) {
        m_enabled = e;
    }

    bool System::IsEnabled() const {
        return m_enabled;
    }

    const std::string& System::GetName() const {
        return m_name;
    }

    void System::Trace(const std::string& msg) {
        std::cout << "[System] " << msg << std::endl;
    }

} // namespace Systems
