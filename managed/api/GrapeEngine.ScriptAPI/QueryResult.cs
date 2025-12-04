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

namespace GrapeEngine;

/// <summary>
/// Result tuple for single-component query.
/// </summary>
public readonly ref struct QueryResult<T1>
    where T1 : unmanaged
{
    public readonly Entity Entity;
    public readonly ref T1 Component1;

    internal unsafe QueryResult(Entity entity, T1* c1)
    {
        Entity = entity;
        Component1 = ref *c1;
    }

    public void Deconstruct(out Entity entity, out T1 c1)
    {
        entity = Entity;
        c1 = Component1;
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

    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
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

    internal unsafe QueryResult(Entity entity, T1* c1, T2* c2, T3* c3)
    {
        Entity = entity;
        Component1 = ref *c1;
        Component2 = ref *c2;
        Component3 = ref *c3;
    }

    public void Deconstruct(out Entity entity, out T1 c1, out T2 c2, out T3 c3)
    {
        entity = Entity;
        c1 = Component1;
        c2 = Component2;
        c3 = Component3;
    }
}

// Add QueryResult<T1, T2, T3, T4> and more as needed...
