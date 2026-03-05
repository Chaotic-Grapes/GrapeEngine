/* Start Header *****************************************************************/
/*!
\file   Systems.cs
\brief  Managed API for enabling/disabling ECS systems at runtime.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Services;

public static class Systems
{
    /// <summary>
    /// Queue a system enabled-state change for the next frame boundary.
    /// </summary>
    /// <param name="systemName">System metadata name.</param>
    /// <param name="enabled">Desired enabled state.</param>
    /// <returns>True if the system name exists, false otherwise.</returns>
    public static bool SetEnabled(string systemName, bool enabled)
    {
        if (string.IsNullOrWhiteSpace(systemName))
        {
            return false;
        }
        return SystemAPI.SetEnabled(systemName, enabled);
    }

    /// <summary>
    /// Get the current effective enabled state of a system.
    /// </summary>
    /// <param name="systemName">System metadata name.</param>
    /// <returns>True if enabled, false if disabled or unknown.</returns>
    public static bool IsEnabled(string systemName)
    {
        if (string.IsNullOrWhiteSpace(systemName))
        {
            return false;
        }
        return SystemAPI.IsEnabled(systemName);
    }
}

