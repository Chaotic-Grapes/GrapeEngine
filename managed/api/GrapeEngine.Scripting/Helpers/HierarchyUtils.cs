/* Start Header *****************************************************************/
/*!
\file   HierarchyUtils.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th November 2025
\brief
Defines the HierarchyUtils helper class for manipulating entity hierarchies.

\details
Provides utility functions for working with entity parent-child relationships
in the ECS world, such as retrieving descendants, finding root ancestors, checking
ancestry, detaching and reparenting entities, and searching for children by name.

\code
// 
var descendants = HierarchyUtils.GetAllDescendants(world, parentEntity);
var root = HierarchyUtils.GetRoot(world, someEntity);

bool isAncestor = HierarchyUtils.IsAncestorOf(world, potentialAncestor, someEntity);

int depth = HierarchyUtils.GetDepth(world, someEntity);

HierarchyUtils.DetachHierarchy(world, entityToDetach);
HierarchyUtils.Reparent(world, entityToMove, newParentEntity);
var child = HierarchyUtils.FindChild(world, parentEntity, "ChildName");
var childRecursive = HierarchyUtils.FindChildRecursive(world, parentEntity, "GrandchildName");
\endcode

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Helpers;

/// <summary>
/// Utility class for working with entity parent-child relationships.
/// </summary>
public static class HierarchyUtils
{
    /// <summary>
    /// Recursively collect all descendants of an entity.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The root entity</param>
    /// <returns>List of all descendants (children, grandchildren, etc.)</returns>
    public static List<Entity> GetAllDescendants(World world, Entity entity)
    {
        var descendants = new List<Entity>();
        CollectDescendantsRecursive(world, entity, descendants);
        return descendants;
    }

    private static void CollectDescendantsRecursive(World world, Entity entity, List<Entity> descendants)
    {
        var children = world.GetChildren(entity);
        foreach (var child in children)
        {
            descendants.Add(child);
            CollectDescendantsRecursive(world, child, descendants);
        }
    }

    /// <summary>
    /// Get the root ancestor of an entity (the top-most parent).
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The entity</param>
    /// <returns>The root ancestor entity, or the entity itself if it has no parent</returns>
    public static Entity GetRoot(World world, Entity entity)
    {
        var current = entity;
        while (true)
        {
            var parent = world.GetParent(current);
            if (parent == null)
                return current;
            current = parent;
        }
    }

    /// <summary>
    /// Check if an entity is an ancestor of another entity.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="potentialAncestor">The potential ancestor</param>
    /// <param name="entity">The entity to check</param>
    /// <returns>True if potentialAncestor is an ancestor of entity</returns>
    public static bool IsAncestorOf(World world, Entity potentialAncestor, Entity entity)
    {
        var current = entity;
        while (true)
        {
            var parent = world.GetParent(current);
            if (parent == null)
                return false;
            if (parent.Id == potentialAncestor.Id)
                return true;
            current = parent;
        }
    }

    /// <summary>
    /// Get the depth of an entity in the hierarchy (0 if root, 1 if child of root, etc.).
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The entity</param>
    /// <returns>The depth in the hierarchy</returns>
    public static int GetDepth(World world, Entity entity)
    {
        int depth = 0;
        var current = entity;
        while (true)
        {
            var parent = world.GetParent(current);
            if (parent == null)
                return depth;
            depth++;
            current = parent;
        }
    }

    /// <summary>
    /// Detach an entity and all its descendants from the hierarchy.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The entity to detach</param>
    public static void DetachHierarchy(World world, Entity entity)
    {
        var descendants = GetAllDescendants(world, entity);
        world.Detach(entity);
        foreach (var descendant in descendants)
        {
            world.Detach(descendant);
        }
    }

    /// <summary>
    /// Move an entity (with all its descendants) to a new parent.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The entity to move</param>
    /// <param name="newParent">The new parent entity</param>
    public static void Reparent(World world, Entity entity, Entity newParent)
    {
        world.Detach(entity);
        world.Attach(entity, newParent);
    }

    /// <summary>
    /// Find a direct child by name.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="parent">The parent entity to search under</param>
    /// <param name="name">The name to search for</param>
    /// <returns>The found entity, or null if not found</returns>
    public static Entity? FindChild(World world, Entity parent, string name)
    {
        //var children = world.GetChildren(parent);
        //var nameChar = name.ToCharArray();
        //foreach (var child in children)
        //{
        //    if (child.TryGetComponent<Name>(out var nameComponent) &&
        //        nameComponent.Value == nameChar)
        //    {
        //        return child;
        //    }
        //}
        return null;
    }

    /// <summary>
    /// Recursively find an entity by name in the hierarchy.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="parent">The parent entity to search under</param>
    /// <param name="name">The name to search for</param>
    /// <returns>The found entity, or null if not found</returns>
    public static Entity? FindChildRecursive(World world, Entity parent, string name)
    {
        //var children = world.GetChildren(parent);
        //var nameChar = name.ToCharArray();
        //foreach (var child in children)
        //{
        //    if (child.TryGetComponent<Name>(out var nameComponent) &&
        //        nameComponent.Value == nameChar)
        //    {
        //        return child;
        //    }

        //    var found = FindChildRecursive(world, child, name);
        //    if (found != null)
        //        return found;
        //}
        return null;
    }
}