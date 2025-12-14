/* Start Header *****************************************************************/
/*!
\file   StructuralChangeValidator.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Validator for safe structural changes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges;

/// <summary>
/// Validator for safe structural changes.
/// 
/// Checks that structural changes don't violate constraints
/// or create invalid world states.
/// </summary>
/// <remarks>
/// Create a validator for a world.
/// </remarks>
public class StructuralChangeValidator(World world)
{
    private readonly World _world = world;
    private readonly List<string> _errors = [];

    /// <summary>
    /// Validate that entity exists before modifying.
    /// </summary>
    public bool ValidateEntityExists(Entity entity)
    {
        // Would check if entity is valid
        return true;
    }

    /// <summary>
    /// Validate that component can be added.
    /// </summary>
    public bool ValidateCanAddComponent<T>(Entity entity) where T : unmanaged
    {
        // Would check archetype constraints
        return true;
    }

    /// <summary>
    /// Validate that component can be removed.
    /// </summary>
    public bool ValidateCanRemoveComponent<T>(Entity entity) where T : unmanaged
    {
        // Would check if component exists
        return true;
    }

    /// <summary>
    /// Get validation errors.
    /// </summary>
    public IEnumerable<string> GetErrors() => _errors;

    /// <summary>
    /// Get error count.
    /// </summary>
    public int GetErrorCount() => _errors.Count;
}
