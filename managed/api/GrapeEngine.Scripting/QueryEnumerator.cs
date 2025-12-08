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
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, null, 0, null, 0, iterPtr);
            }
        }
    }

    internal QueryEnumerator(World world, uint[] componentHashes, uint[] optionalHashes, uint[] excludeHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        fixed (uint* optionalPtr = (optionalHashes != null && optionalHashes.Length > 0) ? optionalHashes : null)
        fixed (uint* excludePtr = (excludeHashes != null && excludeHashes.Length > 0) ? excludeHashes : null)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, optionalPtr, optionalHashes?.Length ?? 0, excludePtr, excludeHashes?.Length ?? 0, iterPtr);
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
                return new QueryResult<T1>(new Entity(_world, _currentEntityId), c1, (System.IntPtr)iterPtr);
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
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, null, 0, null, 0, iterPtr);
            }
        }
    }

    internal QueryEnumerator(World world, uint[] componentHashes, uint[] optionalHashes, uint[] excludeHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        fixed (uint* optionalPtr = (optionalHashes != null && optionalHashes.Length > 0) ? optionalHashes : null)
        fixed (uint* excludePtr = (excludeHashes != null && excludeHashes.Length > 0) ? excludeHashes : null)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, optionalPtr, optionalHashes?.Length ?? 0, excludePtr, excludeHashes?.Length ?? 0, iterPtr);
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
                return new QueryResult<T1, T2>(new Entity(_world, _currentEntityId), c1, c2, (System.IntPtr)iterPtr);
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
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, null, 0, null, 0, iterPtr);
            }
        }
    }

    internal QueryEnumerator(World world, uint[] componentHashes, uint[] optionalHashes, uint[] excludeHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        fixed (uint* optionalPtr = (optionalHashes != null && optionalHashes.Length > 0) ? optionalHashes : null)
        fixed (uint* excludePtr = (excludeHashes != null && excludeHashes.Length > 0) ? excludeHashes : null)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, optionalPtr, optionalHashes?.Length ?? 0, excludePtr, excludeHashes?.Length ?? 0, iterPtr);
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
                return new QueryResult<T1, T2, T3>(new Entity(_world, _currentEntityId), c1, c2, c3, (System.IntPtr)iterPtr);
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

/// <summary>
/// Enumerator for four-component queries.
/// </summary>
public unsafe struct QueryEnumerator<T1, T2, T3, T4>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
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
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, null, 0, null, 0, iterPtr);
            }
        }
    }

    internal QueryEnumerator(World world, uint[] componentHashes, uint[] optionalHashes, uint[] excludeHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        fixed (uint* optionalPtr = (optionalHashes != null && optionalHashes.Length > 0) ? optionalHashes : null)
        fixed (uint* excludePtr = (excludeHashes != null && excludeHashes.Length > 0) ? excludeHashes : null)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, optionalPtr, optionalHashes?.Length ?? 0, excludePtr, excludeHashes?.Length ?? 0, iterPtr);
            }
        }
    }

    public QueryResult<T1, T2, T3, T4> Current
    {
        get
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                T1* c1 = (T1*)QueryInteropAPI.QueryGetComponent(iterPtr, 0);
                T2* c2 = (T2*)QueryInteropAPI.QueryGetComponent(iterPtr, 1);
                T3* c3 = (T3*)QueryInteropAPI.QueryGetComponent(iterPtr, 2);
                T4* c4 = (T4*)QueryInteropAPI.QueryGetComponent(iterPtr, 3);
                return new QueryResult<T1, T2, T3, T4>(new Entity(_world, _currentEntityId), c1, c2, c3, c4, (System.IntPtr)iterPtr);
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
/// Enumerator for five-component queries.
/// </summary>
public unsafe struct QueryEnumerator<T1, T2, T3, T4, T5>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
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
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, null, 0, null, 0, iterPtr);
            }
        }
    }

    internal QueryEnumerator(World world, uint[] componentHashes, uint[] optionalHashes, uint[] excludeHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        fixed (uint* optionalPtr = (optionalHashes != null && optionalHashes.Length > 0) ? optionalHashes : null)
        fixed (uint* excludePtr = (excludeHashes != null && excludeHashes.Length > 0) ? excludeHashes : null)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, optionalPtr, optionalHashes?.Length ?? 0, excludePtr, excludeHashes?.Length ?? 0, iterPtr);
            }
        }
    }

    public QueryResult<T1, T2, T3, T4, T5> Current
    {
        get
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                T1* c1 = (T1*)QueryInteropAPI.QueryGetComponent(iterPtr, 0);
                T2* c2 = (T2*)QueryInteropAPI.QueryGetComponent(iterPtr, 1);
                T3* c3 = (T3*)QueryInteropAPI.QueryGetComponent(iterPtr, 2);
                T4* c4 = (T4*)QueryInteropAPI.QueryGetComponent(iterPtr, 3);
                T5* c5 = (T5*)QueryInteropAPI.QueryGetComponent(iterPtr, 4);
                return new QueryResult<T1, T2, T3, T4, T5>(new Entity(_world, _currentEntityId), c1, c2, c3, c4, c5, (System.IntPtr)iterPtr);
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
/// Enumerator for six-component queries.
/// </summary>
public unsafe struct QueryEnumerator<T1, T2, T3, T4, T5, T6>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
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
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, null, 0, null, 0, iterPtr);
            }
        }
    }

    internal QueryEnumerator(World world, uint[] componentHashes, uint[] optionalHashes, uint[] excludeHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        fixed (uint* optionalPtr = (optionalHashes != null && optionalHashes.Length > 0) ? optionalHashes : null)
        fixed (uint* excludePtr = (excludeHashes != null && excludeHashes.Length > 0) ? excludeHashes : null)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, optionalPtr, optionalHashes?.Length ?? 0, excludePtr, excludeHashes?.Length ?? 0, iterPtr);
            }
        }
    }

    public QueryResult<T1, T2, T3, T4, T5, T6> Current
    {
        get
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                T1* c1 = (T1*)QueryInteropAPI.QueryGetComponent(iterPtr, 0);
                T2* c2 = (T2*)QueryInteropAPI.QueryGetComponent(iterPtr, 1);
                T3* c3 = (T3*)QueryInteropAPI.QueryGetComponent(iterPtr, 2);
                T4* c4 = (T4*)QueryInteropAPI.QueryGetComponent(iterPtr, 3);
                T5* c5 = (T5*)QueryInteropAPI.QueryGetComponent(iterPtr, 4);
                T6* c6 = (T6*)QueryInteropAPI.QueryGetComponent(iterPtr, 5);
                return new QueryResult<T1, T2, T3, T4, T5, T6>(new Entity(_world, _currentEntityId), c1, c2, c3, c4, c5, c6, (System.IntPtr)iterPtr);
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
/// Enumerator for seven-component queries.
/// </summary>
public unsafe struct QueryEnumerator<T1, T2, T3, T4, T5, T6, T7>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
    where T7 : unmanaged
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
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, null, 0, null, 0, iterPtr);
            }
        }
    }

    internal QueryEnumerator(World world, uint[] componentHashes, uint[] optionalHashes, uint[] excludeHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        fixed (uint* optionalPtr = (optionalHashes != null && optionalHashes.Length > 0) ? optionalHashes : null)
        fixed (uint* excludePtr = (excludeHashes != null && excludeHashes.Length > 0) ? excludeHashes : null)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, optionalPtr, optionalHashes?.Length ?? 0, excludePtr, excludeHashes?.Length ?? 0, iterPtr);
            }
        }
    }

    public QueryResult<T1, T2, T3, T4, T5, T6, T7> Current
    {
        get
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                T1* c1 = (T1*)QueryInteropAPI.QueryGetComponent(iterPtr, 0);
                T2* c2 = (T2*)QueryInteropAPI.QueryGetComponent(iterPtr, 1);
                T3* c3 = (T3*)QueryInteropAPI.QueryGetComponent(iterPtr, 2);
                T4* c4 = (T4*)QueryInteropAPI.QueryGetComponent(iterPtr, 3);
                T5* c5 = (T5*)QueryInteropAPI.QueryGetComponent(iterPtr, 4);
                T6* c6 = (T6*)QueryInteropAPI.QueryGetComponent(iterPtr, 5);
                T7* c7 = (T7*)QueryInteropAPI.QueryGetComponent(iterPtr, 6);
                return new QueryResult<T1, T2, T3, T4, T5, T6, T7>(new Entity(_world, _currentEntityId), c1, c2, c3, c4, c5, c6, c7, (System.IntPtr)iterPtr);
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
/// Enumerator for eight-component queries.
/// </summary>
public unsafe struct QueryEnumerator<T1, T2, T3, T4, T5, T6, T7, T8>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
    where T7 : unmanaged
    where T8 : unmanaged
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
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, null, 0, null, 0, iterPtr);
            }
        }
    }

    internal QueryEnumerator(World world, uint[] componentHashes, uint[] optionalHashes, uint[] excludeHashes)
    {
        _world = world;
        _iterator = default;
        _currentEntityId = 0;

        fixed (uint* hashesPtr = componentHashes)
        fixed (uint* optionalPtr = (optionalHashes != null && optionalHashes.Length > 0) ? optionalHashes : null)
        fixed (uint* excludePtr = (excludeHashes != null && excludeHashes.Length > 0) ? excludeHashes : null)
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                QueryInteropAPI.CreateQuery(world.NativePtr, hashesPtr, componentHashes.Length, optionalPtr, optionalHashes?.Length ?? 0, excludePtr, excludeHashes?.Length ?? 0, iterPtr);
            }
        }
    }

    public QueryResult<T1, T2, T3, T4, T5, T6, T7, T8> Current
    {
        get
        {
            fixed (QueryIterator* iterPtr = &_iterator)
            {
                T1* c1 = (T1*)QueryInteropAPI.QueryGetComponent(iterPtr, 0);
                T2* c2 = (T2*)QueryInteropAPI.QueryGetComponent(iterPtr, 1);
                T3* c3 = (T3*)QueryInteropAPI.QueryGetComponent(iterPtr, 2);
                T4* c4 = (T4*)QueryInteropAPI.QueryGetComponent(iterPtr, 3);
                T5* c5 = (T5*)QueryInteropAPI.QueryGetComponent(iterPtr, 4);
                T6* c6 = (T6*)QueryInteropAPI.QueryGetComponent(iterPtr, 5);
                T7* c7 = (T7*)QueryInteropAPI.QueryGetComponent(iterPtr, 6);
                T8* c8 = (T8*)QueryInteropAPI.QueryGetComponent(iterPtr, 7);
                return new QueryResult<T1, T2, T3, T4, T5, T6, T7, T8>(new Entity(_world, _currentEntityId), c1, c2, c3, c4, c5, c6, c7, c8, (System.IntPtr)iterPtr);
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
