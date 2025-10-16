#pragma once
#include <memory>
#include <string>

namespace Resources {

    class Bank {
    public:
        using Ptr = std::shared_ptr<Bank>;

        static Ptr CreateFromFile(std::string path, std::string name = std::string());

        explicit Bank(std::string path, std::string name)
            : m_path(std::move(path)), m_name(std::move(name)) {
        }

        const std::string& getName() const { return m_name; }
        const std::string& getPath() const { return m_path; }

        void setName(std::string n) { m_name = std::move(n); }
        void setPath(std::string p) { m_path = std::move(p); }

    private:
        static std::string DeriveNameFromPath(const std::string& path);

    private:
        std::string m_path;
        std::string m_name;
    };

} // namespace Resources
