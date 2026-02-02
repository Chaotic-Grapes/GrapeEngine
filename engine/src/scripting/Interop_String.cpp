/* Start Header *****************************************************************/
/*!
\file    Interop_String.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Interop functions for StringId interning and resolution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "ecs/StringTable.h"
#include <cstring>
#include <combaseapi.h>

extern "C" {

    INTEROP_API uint32_t StringInterop_Intern(const char* value) {
        if (!value) {
            return 0;
        }
        return ECS::StringTable::Intern(value);
    }

    INTEROP_API const char* StringInterop_Resolve(uint32_t id) {
        std::string value = ECS::StringTable::Resolve(id);
        if (value.empty()) {
            return nullptr;
        }

        size_t len = value.size() + 1;
        char* buffer = static_cast<char*>(CoTaskMemAlloc(len));
        if (!buffer) {
            return nullptr;
        }
        std::memcpy(buffer, value.c_str(), len);
        return buffer;
    }

    INTEROP_API void StringInterop_FreeString(const char* s) {
        if (!s) return;
        CoTaskMemFree(const_cast<char*>(s));
    }
}
