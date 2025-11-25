/* Start Header *****************************************************************/
/*!
\file   ComponentTypeRegistry.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Component type registry and hashing helper.
*/
/* End Header *******************************************************************/

using System;

namespace GrapeEngine.Scripting;

internal static class ComponentTypeRegistry
{
    // FNV-1a hash function - matches C++ ComponentType::Hash()
    private static uint FNV1aHash(string str)
    {
        // FNV-1a prime and offset
        const uint fnvPrime = 0x01000193;
        const uint fnvOffset = 0x811C9DC5;

        var hash = fnvOffset;
        foreach (var c in str)
        {
            hash ^= c;
            hash *= fnvPrime;
        }

        return hash;
    }

    public static uint GetTypeHash<T>() where T : unmanaged
    {
        var name = typeof(T).Name;
        var hash = FNV1aHash(name);
        return hash;
    }
}
