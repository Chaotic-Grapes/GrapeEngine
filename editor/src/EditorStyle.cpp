/* Start Header *****************************************************************/
/*!
\file   EditorStyle.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Single definition file for EditorStyle globals. This ensures that the FontScale 
variable is defined in exactly one translation unit, preventing linker errors 
about multiple definitions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "EditorStyle.h"

namespace EditorStyle {
    // Single, external definition of FontScale to avoid duplicate-symbols across TUs
    float FontScale = 1.0f;
}
