/* Start Header *****************************************************************/
/*!
\file   UI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th November 2025
\brief
High-level UI service wrapper for scripts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/


using GrapeEngine.Scripting.Unsafe;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Services;

/// <summary>
/// High-level static UI helpers exposed to scripts.
/// </summary>
public static class UI
{
    /// <summary>
    /// Returns true if any UI element was clicked this frame.
    /// </summary>
    public static bool WasAnyClicked() => UIAPI.WasAnyClicked();

    /// <summary>
    /// Returns the ActionID of the last clicked UI element (0 if none).
    /// </summary>
    public static uint GetClickedActionID() => UIAPI.GetClickedActionID();
}
