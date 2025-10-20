#include "core/IService.h"
#include "core/Logger.h"

namespace Engine {
    void IService::Trace(const std::string& msg) {
        LOG_TRACE("[Service] " << msg);
    }
}