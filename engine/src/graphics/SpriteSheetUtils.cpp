/* Start Header *****************************************************************/
/*!
\file   SpriteSheetUtils.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   29th January 2026
\brief
Implements helpers for sprite sheet UV calculation used by ECS systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "graphics/SpriteSheetUtils.h"
#include <algorithm>

namespace SpriteSheetUtils {
    // Calculate the grid layout (columns and rows) of frames in a sprite sheet
    Layout ComputeLayout(int frameWidth, int frameHeight, int sheetWidth, int sheetHeight) {
        Layout layout{};
        
        // Return empty layout if any dimension is invalid
        if (frameWidth <= 0 || frameHeight <= 0 || sheetWidth <= 0 || sheetHeight <= 0)
            return layout;

        // Divide sheet dimensions by frame dimensions to get grid size
        layout.TotalCols = sheetWidth / frameWidth;
        layout.TotalRows = sheetHeight / frameHeight;

        // Clamp negative values (from integer division errors) to zero
        if (layout.TotalCols < 0) layout.TotalCols = 0;
        if (layout.TotalRows < 0) layout.TotalRows = 0;
        return layout;
    }

    // Define a range of frames using a start index and count, with optional auto-expansion to end of sheet
    Window ComputeWindow(int startFrame, int frameCount, int totalCols, int totalRows) {
        Window window{};

        // Return empty window if sheet grid is invalid
        if (totalCols <= 0 || totalRows <= 0)
            return window;

        // Ensure start frame is non-negative
        window.Start = std::max(0, startFrame);
        window.Count = frameCount;

        // If count is not specified or negative, auto-expand to include remaining frames from start position
        if (window.Count <= 0) {
            const int totalFrames = totalCols * totalRows;
            window.Count = std::max(1, totalFrames - window.Start);
        }
        return window;
    }

    // Define a range of consecutive frames within a specific row of the sprite sheet
    Window ComputeRowWindow(int rowIndex, int rowStartCol, int rowFrameCount, int totalCols, int totalRows) {
        Window window{};

        // Return empty window if sheet grid is invalid
        if (totalCols <= 0 || totalRows <= 0)
            return window;

        // Clamp row/column indices; row index is top-down (0 = top row)
        const int row = std::clamp(rowIndex, 0, totalRows - 1);
        const int startCol = std::clamp(rowStartCol, 0, totalCols - 1);

        // Calculate how many frames are available from the start column to the end of the row
        const int available = totalCols - startCol;

        // Use requested frame count or limit to available frames in the row
        int count = rowFrameCount;
        if (count <= 0 || count > available)
            count = available;

        // Calculate the absolute frame index (row-major layout) and frame count
        window.Start = row * totalCols + startCol;
        window.Count = count;
        return window;
    }

    // Compute normalized UV coordinates for a frame at the given absolute position in the sprite sheet
    glm::vec4 ComputeUV(int absoluteFrame, int frameWidth, int frameHeight, int sheetWidth, int sheetHeight) {
        // Return zero UV if dimensions are invalid
        if (frameWidth <= 0 || frameHeight <= 0 || sheetWidth <= 0 || sheetHeight <= 0)
            return glm::vec4(0.0f);

        // Calculate number of columns; return zero if invalid
        const int totalCols = sheetWidth / frameWidth;
        if (totalCols <= 0)
            return glm::vec4(0.0f);

        // Determine row and column from absolute frame index (row-major layout)
        const int col = absoluteFrame % totalCols;
        int row = absoluteFrame / totalCols;
        const int totalRows = sheetHeight / frameHeight;
        row = (totalRows - 1) - row; // Flip so row 0 is the top row.

        // Convert pixel coordinates to normalized UV coordinates (0.0 to 1.0)
        const float u0 = (col * frameWidth) / static_cast<float>(sheetWidth);
        const float v0 = (row * frameHeight) / static_cast<float>(sheetHeight);
        const float u1 = ((col + 1) * frameWidth) / static_cast<float>(sheetWidth);
        const float v1 = ((row + 1) * frameHeight) / static_cast<float>(sheetHeight);

        // Return UV bounds: (u_min, v_min, u_max, v_max)
        return glm::vec4(u0, v0, u1, v1);
    }
}
