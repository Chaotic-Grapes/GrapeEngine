/* Start Header *****************************************************************/
/*!
\file   QueryEnumerator.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Query enumerators for foreach support.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;
using System.Collections;

namespace GrapeEngine;

/// <summary>
/// Enumerator for single-component queries.
/// </summary>
public unsafe struct QueryEnumerator<T1>
    where T1 : unmanaged
{
    private readonly World _world;
    private QueryIterator _iterator;
    private readonly ulong _currentEntityId;

    internal QueryEnumerator(World world, uint[] componentHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        // Initialize query iterator
        fixed (uint* hashesPtr = componentHashes)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, iterPtr);
            }
        }
    }

    public QueryResult<T1> Current
    {
        get
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                T1* c1 = (T1*)QueryInteropAPI.QueryGetComponent(iterPtr, 0);
                return new QueryResult<T1>(new Entity(_world, _currentEntityId), c1);
            }
        }
    }

    public bool MoveNext()
    {
        fixed (QueryIterator* iterPtr = &_iterator)
        {
            fixed (ulong* entityPtr = &_currentEntityId)
            {
                return QueryInteropAPI.QueryNext(iterPtr, entityPtr);
            }
        }
    }

    public void Reset() => throw new NotSupportedException();
    public void Dispose() { }
}

/// <summary>
/// Enumerator for two-component queries.
/// </summary>
public unsafe struct QueryEnumerator<T1, T2>
    where T1 : unmanaged
    where T2 : unmanaged
{
    private readonly World _world;
    private QueryIterator _iterator;
    private readonly ulong _currentEntityId;

    internal QueryEnumerator(World world, uint[] componentHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, iterPtr);
            }
        }
    }

    public QueryResult<T1, T2> Current
    {
        get
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                T1* c1 = (T1*)QueryInteropAPI.QueryGetComponent(iterPtr, 0);
                T2* c2 = (T2*)QueryInteropAPI.QueryGetComponent(iterPtr, 1);
                return new QueryResult<T1, T2>(new Entity(_world, _currentEntityId), c1, c2);
            }
        }
    }

    public bool MoveNext()
    {
        fixed (QueryIterator* iterPtr = &_iterator)
        {
            fixed (ulong* entityPtr = &_currentEntityId)
            {
                return QueryInteropAPI.QueryNext(iterPtr, entityPtr);
            }
        }
    }

    public void Reset() => throw new NotSupportedException();
    public void Dispose() { }
}

/// <summary>
/// Enumerator for three-component queries.
/// </summary>
public unsafe struct QueryEnumerator<T1, T2, T3>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
{
    private readonly World _world;
    private QueryIterator _iterator;
    private readonly ulong _currentEntityId;

    internal QueryEnumerator(World world, uint[] componentHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, iterPtr);
            }
        }
    }

    public QueryResult<T1, T2, T3> Current
    {
        get
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                T1* c1 = (T1*)QueryInteropAPI.QueryGetComponent(iterPtr, 0);
                T2* c2 = (T2*)QueryInteropAPI.QueryGetComponent(iterPtr, 1);
                T3* c3 = (T3*)QueryInteropAPI.QueryGetComponent(iterPtr, 2);
                return new QueryResult<T1, T2, T3>(new Entity(_world, _currentEntityId), c1, c2, c3);
            }
        }
    }

    public bool MoveNext()
    {
        fixed (QueryIterator* iterPtr = &_iterator)
        {
            fixed (ulong* entityPtr = &_currentEntityId)
            {
                return QueryInteropAPI.QueryNext(iterPtr, entityPtr);
            }
        }
    }

    public void Reset() => throw new NotSupportedException();
    public void Dispose() { }
}

// Add QueryEnumerator<T1, T2, T3, T4> and more as needed...
