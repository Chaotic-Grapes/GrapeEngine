/* Start Header *****************************************************************/
/*!
\file    CompilePanel.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file declares the CompilePanel class which is responsible for displaying 
compilation status, progress, and logging diagnostics to the engine logger.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include <string>

class CompilePanel {
public:
    static void Initialize();
    static void Shutdown();
    static void Render();
};
