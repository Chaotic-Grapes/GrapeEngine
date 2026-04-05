/* Start Header *****************************************************************/
/*!
\file   EditorCallbacks.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of the EditorCallbackRegistry singleton.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "core/EditorCallbacks.h"

namespace Engine {

    // Function-local static keeps singleton initialization thread-safe in modern C++.
    EditorCallbackRegistry& EditorCallbackRegistry::Get() {
        static EditorCallbackRegistry instance;
        return instance;
    }

}
