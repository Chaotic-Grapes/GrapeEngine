#pragma once
#include <string>

namespace Systems {

    class System {
    public:
        explicit System(std::string name);
        virtual ~System();

        // Lifecycle hooks
        virtual void Initialize();
        virtual void Update(float dt);
        virtual void Terminate();

        // Enable/disable the system (engine can skip Update when disabled)
        void SetEnabled(bool e);
        bool IsEnabled() const;

        // Introspection
        const std::string& GetName() const;

    protected:
        // Simple log helper available to derived systems (e.g., Audio)
        static void Trace(const std::string& msg);

    private:
        std::string m_name;
        bool        m_enabled;
    };

} // namespace Systems
