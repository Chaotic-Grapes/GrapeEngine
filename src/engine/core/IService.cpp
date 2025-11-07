/* Start Header *****************************************************************/
/*!
\file   IService.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   5th October 2025
\brief
Implements the IService interface which serves as a base class for all
services within the engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "core/IService.h"
#include "core/Logger.h"

namespace Engine {
    void IService::Trace(const std::string& msg) {
        LOG_TRACE("[Service] " << msg);
    }
}