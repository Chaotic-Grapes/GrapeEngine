/* Start Header *****************************************************************/
/*!
\file   Query.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Query API for iterating over entities with specific components.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Collections;

namespace GrapeEngine;

/// <summary>
/// Query for iterating over entities with one component type.
/// </summary>
public unsafe class Query<T1> : IEnumerable<QueryResult<T1>>
    where T1 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>()
        };
    }

    public QueryEnumerator<T1> GetEnumerator()
    {
        return new QueryEnumerator<T1>(_world, _componentHashes);
    }

    IEnumerator<QueryResult<T1>> IEnumerable<QueryResult<T1>>.GetEnumerator() => GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}

/// <summary>
/// Query for iterating over entities with two component types.
/// </summary>
public unsafe class Query<T1, T2> : IEnumerable<QueryResult<T1, T2>>
    where T1 : unmanaged
    where T2 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>()
        };
    }

    public QueryEnumerator<T1, T2> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2>(_world, _componentHashes);
    }

    IEnumerator<QueryResult<T1, T2>> IEnumerable<QueryResult<T1, T2>>.GetEnumerator() => GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}

/// <summary>
/// Query for iterating over entities with three component types.
/// </summary>
public unsafe class Query<T1, T2, T3> : IEnumerable<QueryResult<T1, T2, T3>>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        ComponentRegistry.EnsureRegistered<T3>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>(),
            ComponentTypeHelper.GetTypeHash<T3>()
        };
    }

    public QueryEnumerator<T1, T2, T3> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2, T3>(_world, _componentHashes);
    }

    IEnumerator<QueryResult<T1, T2, T3>> IEnumerable<QueryResult<T1, T2, T3>>.GetEnumerator() => GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}

// Add Query<T1, T2, T3, T4> and more as needed...
