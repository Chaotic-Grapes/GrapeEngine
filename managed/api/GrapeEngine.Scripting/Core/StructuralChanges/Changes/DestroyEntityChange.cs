/* Start Header *****************************************************************/
/*!
\file   DestroyEntityChange.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Deferred entity destruction change.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Changes;

/// <summary>
/// Deferred entity destruction change.
/// </summary>
public class DestroyEntityChange : IChangeHandler
{
    /// <summary>
    /// Entity to destroy
    /// </summary>
    public required Entity TargetEntity { get; set; }

    public void Apply(World world)
    {
        world.DestroyEntity(TargetEntity);
    }

    public string GetDescription() => $"DestroyEntity({TargetEntity.Id})";
}

