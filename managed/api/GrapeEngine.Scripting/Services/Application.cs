/* Start Header *****************************************************************/
/*!
\file   Application.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
Provides access to application-level functionality and configuration.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/


using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine;

/// <summary>
/// Provides access to application-level functionality and configuration.
/// </summary>
public static class Application
{
    /// <summary>
    /// Request the application to quit.
    /// </summary>
    public static void Quit() => ApplicationAPI.Quit();

    /// <summary>
    /// Get the application or game name from configuration.
    /// </summary>
    public static string Name => ApplicationAPI.GetName();

    /// <summary>
    /// Get the fixed time step from physics configuration.
    /// </summary>
    public static float FixedTimeStep => ApplicationAPI.GetFixedTimeStep();

    /// <summary>
    /// Check if VSync is enabled in the configuration.
    /// </summary>
    public static bool IsVSyncEnabled => ApplicationAPI.IsVSyncEnabled();
}
