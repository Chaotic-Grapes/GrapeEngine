/* Start Header *****************************************************************/
/*!
\file   IChangeHandler.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Base interface for deferred change handlers.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Changes;

/// <summary>
/// Base interface for deferred change handlers.
/// </summary>
public interface IChangeHandler
{
    /// <summary>
    /// Apply the change to the world
    /// </summary>
    void Apply(World world);

    /// <summary>
    /// Get human-readable description
    /// </summary>
    string GetDescription();
}

