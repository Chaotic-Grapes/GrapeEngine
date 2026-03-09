/* Start Header *****************************************************************/
/*!
\file   SpriteSheetUtils.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   29th January 2026
\brief
Small helpers for sprite sheet UV calculation used by ECS systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SPRITESHEETUTILS_H
#define SPRITESHEETUTILS_H

#include <glm/glm.hpp>

namespace SpriteSheetUtils {
    
    /**
     * @brief Represents a window of frames in the sprite sheet
     */
    struct Window {
        int Start = 0; // Starting frame index
        int Count = 0; // Number of frames
    };

    /**
     * @brief Layout of the sprite sheet in terms of total columns and rows
     */
    struct Layout {
        int TotalCols = 0; // Number of columns
        int TotalRows = 0; // Number of rows
    };

    /**
     * @brief Compute layout (total columns and rows) of the sprite sheet
     * @param frameWidth Width of a single frame in pixels
     * @param frameHeight Height of a single frame in pixels
     * @param sheetWidth Width of the entire sprite sheet in pixels
     * @param sheetHeight Height of the entire sprite sheet in pixels
     * @return Layout struct with total columns and rows
     */
    Layout ComputeLayout(int frameWidth, int frameHeight, int sheetWidth, int sheetHeight);
    
    /**
     * @brief Compute a window (start frame and count) for a sequence of frames
     * @param startFrame Index of the starting frame (0-based)
     * @param frameCount Number of frames in the sequence
     * @param totalCols Total columns in the sprite sheet
     * @param totalRows Total rows in the sprite sheet
     * @return Window struct with start and count
     */
    Window ComputeWindow(int startFrame, int frameCount, int totalCols, int totalRows);

    /**
     * @brief Compute a window from a row/column anchor in the sprite sheet
     * @param rowIndex Index of the row (0-based)
     * @param rowStartCol Starting column index for the row (0-based)
     * @param rowFrameCount Number of frames to include.
     *        - <= 0: use the rest of the selected row
     *        - > 0 : may continue into following rows
     * @param totalCols Total columns in the sprite sheet
     * @param totalRows Total rows in the sprite sheet
     * @return Window struct with start and count
     */
    Window ComputeRowWindow(int rowIndex, int rowStartCol, int rowFrameCount, int totalCols, int totalRows);

    /**
     * @brief Compute UV coordinates for a given absolute frame index
     * @param absoluteFrame Absolute frame index (0-based)
     * @param frameWidth Width of a single frame in pixels
     * @param frameHeight Height of a single frame in pixels
     * @param sheetWidth Width of the entire sprite sheet in pixels
     * @param sheetHeight Height of the entire sprite sheet in pixels
     * @return glm::vec4 containing (uMin, vMin, uMax, vMax)
     */
    glm::vec4 ComputeUV(int absoluteFrame, int frameWidth, int frameHeight, int sheetWidth, int sheetHeight);
}

#endif
