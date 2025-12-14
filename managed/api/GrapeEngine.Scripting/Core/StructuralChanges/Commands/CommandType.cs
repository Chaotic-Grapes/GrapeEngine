/* Start Header *****************************************************************/
/*!
\file   CommandType.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Type of structural change command.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Commands;

/// <summary>
/// Type of structural change command.
/// </summary>
public enum CommandType
{
    /// <summary>
    /// Create a new entity
    /// </summary>
    CreateEntity,

    /// <summary>
    /// Destroy an entity
    /// </summary>
    DestroyEntity,

    /// <summary>
    /// Add component to entity
    /// </summary>
    AddComponent,

    /// <summary>
    /// Remove component from entity
    /// </summary>
    RemoveComponent,

    /// <summary>
    /// Set component value on entity
    /// </summary>
    SetComponent,

    /// <summary>
    /// Instantiate entity from archetype
    /// </summary>
    InstantiateEntity,

    /// <summary>
    /// Clone existing entity
    /// </summary>
    CloneEntity
}
