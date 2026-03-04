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

        const int totalFrames = totalCols * totalRows;

        // Ensure start frame is non-negative
        window.Start = std::clamp(startFrame, 0, totalFrames - 1);
        window.Count = frameCount;

        // If count is not specified or negative, auto-expand to include remaining frames from start position
        if (window.Count <= 0) {
            window.Count = std::max(1, totalFrames - window.Start);
        }
        else {
            window.Count = std::min(window.Count, totalFrames - window.Start);
        }
        return window;
    }

    // Define a range of consecutive frames starting from a row/column position.
    // Row index is interpreted directly in sheet row-major order.
    Window ComputeRowWindow(int rowIndex, int rowStartCol, int rowFrameCount, int totalCols, int totalRows) {
        Window window{};

        // Return empty window if sheet grid is invalid
        if (totalCols <= 0 || totalRows <= 0)
            return window;

        // Clamp row/column indices directly in row-major index space.
        const int row = std::clamp(rowIndex, 0, totalRows - 1);
        const int startCol = std::clamp(rowStartCol, 0, totalCols - 1);
        const int start = row * totalCols + startCol;
        const int totalFrames = totalCols * totalRows;

        // Frames available from start to end-of-row; used by FrameLength=0 default.
        const int rowAvailable = totalCols - startCol;
        const int maxFromStart = totalFrames - start;

        // FrameLength semantics:
        // - <= 0 : consume only the rest of the selected row
        // - > 0  : allow spanning into following rows, clamped to sheet end
        int count = rowFrameCount;
        if (count <= 0) {
            count = rowAvailable;
        }
        else {
            count = std::clamp(count, 1, maxFromStart);
        }

        // Calculate the absolute frame index (row-major layout) and frame count.
        window.Start = start;
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
        const int totalRows = sheetHeight / frameHeight;
        if (totalCols <= 0 || totalRows <= 0)
            return glm::vec4(0.0f);

        // Determine row/column from absolute frame index in top-first row-major layout.
        const int col = absoluteFrame % totalCols;
        const int rowTop = absoluteFrame / totalCols;
        const int rowBottom = (totalRows - 1) - rowTop;

        // Convert pixel coordinates to normalized UV coordinates (0.0 to 1.0)
        const float u0 = (col * frameWidth) / static_cast<float>(sheetWidth);
        const float v0 = (rowBottom * frameHeight) / static_cast<float>(sheetHeight);
        const float u1 = ((col + 1) * frameWidth) / static_cast<float>(sheetWidth);
        const float v1 = ((rowBottom + 1) * frameHeight) / static_cast<float>(sheetHeight);

        // Return UV bounds: (u_min, v_min, u_max, v_max)
        return glm::vec4(u0, v0, u1, v1);
    }
}
