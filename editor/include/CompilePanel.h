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
    /**
     * @brief Initialize compile panel state and resources.
     */
    static void Initialize();

    /**
     * @brief Shutdown compile panel state and release resources.
     */
    static void Shutdown();

    /**
     * @brief Render compile status and diagnostics UI.
     */
    static void Render();
};
