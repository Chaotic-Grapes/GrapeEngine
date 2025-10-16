
#include "audio/AudioBank.h"
#include <algorithm>

namespace {
    static inline std::string FilenameOnly(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        return (pos == std::string::npos) ? path : path.substr(pos + 1);
    }
    static inline std::string StripExt(const std::string& name) {
        size_t dot = name.find_last_of('.');
        return (dot == std::string::npos) ? name : name.substr(0, dot);
    }
} // namespace

namespace Resources {

    /*static*/ Bank::Ptr Bank::CreateFromFile(std::string path, std::string name) {
        if (name.empty()) name = DeriveNameFromPath(path);
        return std::make_shared<Bank>(std::move(path), std::move(name));
    }

    std::string Bank::DeriveNameFromPath(const std::string& path) {
        return StripExt(FilenameOnly(path));
    }

} // namespace Resources
