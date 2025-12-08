/* Start Header *****************************************************************/
/*!
\file   QueryResult.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Query result structures returned by query iteration.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Query;

/// <summary>
/// Result tuple for single-component query.
/// </summary>
public readonly ref struct QueryResult<T1>
    where T1 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;
    private readonly System.IntPtr _iteratorPtr;

    internal unsafe QueryResult(Entity entity, T1* c1, System.IntPtr iteratorPtr)
    {
        Entity = entity;
        Component1 = ref *c1;
        _iteratorPtr = iteratorPtr;
    }

    public void Deconstruct(out Entity entity, out T1 c1)
    {
        entity = Entity;
        c1 = Component1;
    }

    public QueryOptional<TOpt> GetOptional<TOpt>() where TOpt : unmanaged
    {
        if (_iteratorPtr == System.IntPtr.Zero) return new QueryOptional<TOpt>(System.IntPtr.Zero);
        uint hash = ComponentTypeHelper.GetTypeHash<TOpt>();
        unsafe
        {
            var iterPtr = (GrapeEngine.Scripting.Unsafe.QueryIterator*)_iteratorPtr;
            void* ptr = QueryInteropAPI.QueryGetOptionalComponent(iterPtr, hash);
            return new QueryOptional<TOpt>((System.IntPtr)ptr);
        }
    }
}

/// <summary>
/// Result tuple for two-component query.
/// </summary>
public readonly ref struct QueryResult<T1, T2>
    where T1 : unmanaged
    where T2 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;
    public readonly ref T2 Component2;
    private readonly System.IntPtr _iteratorPtr;
    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2, System.IntPtr iteratorPtr)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
        _iteratorPtr = iteratorPtr;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
    }

    public QueryOptional<TOpt> GetOptional<TOpt>() where TOpt : unmanaged
    {
        if (_iteratorPtr == System.IntPtr.Zero)
            return new QueryOptional<TOpt>(System.IntPtr.Zero);
        
        uint hash = ComponentTypeHelper.GetTypeHash<TOpt>();
        
        unsafe
        {
            var iterPtr = (GrapeEngine.Scripting.Unsafe.QueryIterator*)_iteratorPtr;
            void* ptr = QueryInteropAPI.QueryGetOptionalComponent(iterPtr, hash);
            return new QueryOptional<TOpt>((System.IntPtr)ptr);
        }
    }
}

/// <summary>
/// Result tuple for three-component query.
/// </summary>
public readonly ref struct QueryResult<T1, T2, T3>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;
    public readonly ref T2 Component2;
    public readonly ref T3 Component3;

    private readonly System.IntPtr _iteratorPtr;

    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2, T3* c3, System.IntPtr iteratorPtr)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
        Component3 = ref *c3;
        _iteratorPtr = iteratorPtr;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2, out T3 c3)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
        c3 = Component3;
    }

    public QueryOptional<TOpt> GetOptional<TOpt>() where TOpt : unmanaged
    {
        if (_iteratorPtr == System.IntPtr.Zero) 
            return new QueryOptional<TOpt>(System.IntPtr.Zero);
            
        uint hash = ComponentTypeHelper.GetTypeHash<TOpt>();

        unsafe
        {
            var iterPtr = (GrapeEngine.Scripting.Unsafe.QueryIterator*)_iteratorPtr;
            void* ptr = QueryInteropAPI.QueryGetOptionalComponent(iterPtr, hash);
            return new QueryOptional<TOpt>((System.IntPtr)ptr);
        }
    }
}

// Add QueryResult<T1, T2, T3, T4> and more as needed...

/// <summary>
/// Result tuple for four-component query.
/// </summary>
public readonly ref struct QueryResult<T1, T2, T3, T4>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;
    public readonly ref T2 Component2;
    public readonly ref T3 Component3;
    public readonly ref T4 Component4;

    private readonly System.IntPtr _iteratorPtr;

    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2, T3* c3, T4* c4, System.IntPtr iteratorPtr)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
        Component3 = ref *c3;
        Component4 = ref *c4;
        _iteratorPtr = iteratorPtr;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2, out T3 c3, out T4 c4)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
        c3 = Component3;
        c4 = Component4;
    }

    public QueryOptional<TOpt> GetOptional<TOpt>() where TOpt : unmanaged
    {
        if (_iteratorPtr == System.IntPtr.Zero) 
            return new QueryOptional<TOpt>(System.IntPtr.Zero);

        uint hash = ComponentTypeHelper.GetTypeHash<TOpt>();

        unsafe
        {
            var iterPtr = (GrapeEngine.Scripting.Unsafe.QueryIterator*)_iteratorPtr;
            void* ptr = QueryInteropAPI.QueryGetOptionalComponent(iterPtr, hash);
            return new QueryOptional<TOpt>((System.IntPtr)ptr);
        }
    }
}

/// <summary>
/// Wrapper for optional component values returned by queries.
/// </summary>
public readonly ref struct QueryOptional<T>
    where T : unmanaged
{
    private readonly System.IntPtr _ptr;

    internal QueryOptional(System.IntPtr ptr)
    {
        _ptr = ptr;
    }

    public bool HasValue => _ptr != System.IntPtr.Zero;

    public bool TryGet(out T value)
    {
        if (_ptr == System.IntPtr.Zero)
        {
            value = default;
            return false;
        }
        unsafe
        {
            value = *(T*)_ptr;
            return true;
        }
    }

    public T GetValueOrDefault()
    {
        if (_ptr == System.IntPtr.Zero) return default;
        unsafe { return *(T*)_ptr; }
    }
}

// Result tuple for five-component query.
public readonly ref struct QueryResult<T1, T2, T3, T4, T5>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;
    public readonly ref T2 Component2;
    public readonly ref T3 Component3;
    public readonly ref T4 Component4;
    public readonly ref T5 Component5;
    private readonly System.IntPtr _iteratorPtr;

    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2, T3* c3, T4* c4, T5* c5, System.IntPtr iteratorPtr)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
        Component3 = ref *c3;
        Component4 = ref *c4;
        Component5 = ref *c5;
        _iteratorPtr = iteratorPtr;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2, out T3 c3, out T4 c4, out T5 c5)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
        c3 = Component3;
        c4 = Component4;
        c5 = Component5;
    }

    public QueryOptional<TOpt> GetOptional<TOpt>() where TOpt : unmanaged
    {
        if (_iteratorPtr == System.IntPtr.Zero)
            return new QueryOptional<TOpt>(System.IntPtr.Zero);
        
        uint hash = ComponentTypeHelper.GetTypeHash<TOpt>();

        unsafe
        {
            var iterPtr = (GrapeEngine.Scripting.Unsafe.QueryIterator*)_iteratorPtr;
            void* ptr = QueryInteropAPI.QueryGetOptionalComponent(iterPtr, hash);
            return new QueryOptional<TOpt>((System.IntPtr)ptr);
        }
    }
}

// Result tuple for six-component query.
public readonly ref struct QueryResult<T1, T2, T3, T4, T5, T6>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;
    public readonly ref T2 Component2;
    public readonly ref T3 Component3;
    public readonly ref T4 Component4;
    public readonly ref T5 Component5;
    public readonly ref T6 Component6;
    private readonly System.IntPtr _iteratorPtr;

    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2, T3* c3, T4* c4, T5* c5, T6* c6, System.IntPtr iteratorPtr)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
        Component3 = ref *c3;
        Component4 = ref *c4;
        Component5 = ref *c5;
        Component6 = ref *c6;
        _iteratorPtr = iteratorPtr;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2, out T3 c3, out T4 c4, out T5 c5, out T6 c6)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
        c3 = Component3;
        c4 = Component4;
        c5 = Component5;
        c6 = Component6;
    }

    public QueryOptional<TOpt> GetOptional<TOpt>() where TOpt : unmanaged
    {
        if (_iteratorPtr == System.IntPtr.Zero)
            return new QueryOptional<TOpt>(System.IntPtr.Zero);

        uint hash = ComponentTypeHelper.GetTypeHash<TOpt>();

        unsafe
        {
            var iterPtr = (GrapeEngine.Scripting.Unsafe.QueryIterator*)_iteratorPtr;
            void* ptr = QueryInteropAPI.QueryGetOptionalComponent(iterPtr, hash);
            return new QueryOptional<TOpt>((System.IntPtr)ptr);
        }
    }
}

// Result tuple for seven-component query.
public readonly ref struct QueryResult<T1, T2, T3, T4, T5, T6, T7>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
    where T7 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;
    public readonly ref T2 Component2;
    public readonly ref T3 Component3;
    public readonly ref T4 Component4;
    public readonly ref T5 Component5;
    public readonly ref T6 Component6;
    public readonly ref T7 Component7;
    private readonly System.IntPtr _iteratorPtr;

    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2, T3* c3, T4* c4, T5* c5, T6* c6, T7* c7, System.IntPtr iteratorPtr)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
        Component3 = ref *c3;
        Component4 = ref *c4;
        Component5 = ref *c5;
        Component6 = ref *c6;
        Component7 = ref *c7;
        _iteratorPtr = iteratorPtr;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2, out T3 c3, out T4 c4, out T5 c5, out T6 c6, out T7 c7)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
        c3 = Component3;
        c4 = Component4;
        c5 = Component5;
        c6 = Component6;
        c7 = Component7;
    }

    public QueryOptional<TOpt> GetOptional<TOpt>() where TOpt : unmanaged
    {
        if (_iteratorPtr == System.IntPtr.Zero)
            return new QueryOptional<TOpt>(System.IntPtr.Zero);

        uint hash = ComponentTypeHelper.GetTypeHash<TOpt>();

        unsafe
        {
            var iterPtr = (GrapeEngine.Scripting.Unsafe.QueryIterator*)_iteratorPtr;
            void* ptr = QueryInteropAPI.QueryGetOptionalComponent(iterPtr, hash);
            return new QueryOptional<TOpt>((System.IntPtr)ptr);
        }
    }
}

// Result tuple for eight-component query.
public readonly ref struct QueryResult<T1, T2, T3, T4, T5, T6, T7, T8>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
    where T7 : unmanaged
    where T8 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;
    public readonly ref T2 Component2;
    public readonly ref T3 Component3;
    public readonly ref T4 Component4;
    public readonly ref T5 Component5;
    public readonly ref T6 Component6;
    public readonly ref T7 Component7;
    public readonly ref T8 Component8;
    private readonly System.IntPtr _iteratorPtr;

    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2, T3* c3, T4* c4, T5* c5, T6* c6, T7* c7, T8* c8, System.IntPtr iteratorPtr)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
        Component3 = ref *c3;
        Component4 = ref *c4;
        Component5 = ref *c5;
        Component6 = ref *c6;
        Component7 = ref *c7;
        Component8 = ref *c8;
        _iteratorPtr = iteratorPtr;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2, out T3 c3, out T4 c4, out T5 c5, out T6 c6, out T7 c7, out T8 c8)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
        c3 = Component3;
        c4 = Component4;
        c5 = Component5;
        c6 = Component6;
        c7 = Component7;
        c8 = Component8;
    }

    public QueryOptional<TOpt> GetOptional<TOpt>() where TOpt : unmanaged
    {
        if (_iteratorPtr == System.IntPtr.Zero)
            return new QueryOptional<TOpt>(System.IntPtr.Zero);

        uint hash = ComponentTypeHelper.GetTypeHash<TOpt>();

        unsafe
        {
            var iterPtr = (GrapeEngine.Scripting.Unsafe.QueryIterator*)_iteratorPtr;
            void* ptr = QueryInteropAPI.QueryGetOptionalComponent(iterPtr, hash);
            return new QueryOptional<TOpt>((System.IntPtr)ptr);
        }
    }
}
