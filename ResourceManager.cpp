#include "ResourceManager.h"
using RM = ResourceManager;

// Template specialization for Texture
template<>
std::shared_ptr<Texture> RM::Get<Texture>(const std::string& name) {
    return nullptr;
}

// Template specialization for Audio
template<>
std::shared_ptr<Audio> RM::Get<Audio>(const std::string& name) {
    return nullptr;
}
