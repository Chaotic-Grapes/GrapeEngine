/* Start Header *****************************************************************/
/*!
\file   CreateEntityChange.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Deferred entity creation change.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Changes;

/// <summary>
/// Deferred entity creation change.
/// </summary>
public class CreateEntityChange : IChangeHandler
{
    /// <summary>
    /// Archetype for new entity (optional)
    /// </summary>
    public uint? ArchetypeId { get; set; }

    /// <summary>
    /// Initial components to add (optional)
    /// </summary>
    public Dictionary<Type, object> InitialComponents { get; set; } = [];

    public void Apply(World world)
    {
        var entity = world.CreateEntity();

        if (InitialComponents != null)
        {
            foreach (var (componentType, componentData) in InitialComponents)
            {
                try
                {
                    var method = typeof(World).GetMethod("AddComponent")
                        ?.MakeGenericMethod(componentType);

                    if (method != null)
                    {
                        method.Invoke(world, [entity, componentData]);
                    }
                    else
                    {
                        Logging.LogInternal($"[DeferredChanges] CreateEntity failed: Could not find AddComponent method for {componentType.Name}", LogLevel.Warning);
                    }
                }
                catch (Exception ex)
                {
                    Logging.LogInternal($"[DeferredChanges] CreateEntity error adding {componentType.Name}: {ex.Message}", LogLevel.Error);
                }
            }
        }
    }

    public string GetDescription() => $"CreateEntity(archetype={ArchetypeId ?? 0}, components={InitialComponents?.Count ?? 0})";
}
