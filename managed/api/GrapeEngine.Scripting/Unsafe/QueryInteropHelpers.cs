using System;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

internal static unsafe class QueryInteropHelpers
{
    // Centralized CreateQuery helper that pins managed arrays and calls the native API.
    internal static void CreateQueryWithArrays(void* worldPtr, uint[] required, uint[]? optional, uint[]? exclude, out QueryIterator iterator)
    {
        iterator = default;

        fixed (uint* reqPtr = required)
        {
            uint* optionalPtr = null;
            uint* excludePtr = null;

            if (optional != null && optional.Length > 0)
            {
                fixed (uint* op = optional)
                {
                    optionalPtr = op;
                    if (exclude != null && exclude.Length > 0)
                    {
                        fixed (uint* ep = exclude)
                        {
                            excludePtr = ep;
                            fixed (QueryIterator* iterPtr = &iterator)
                            {
                                QueryInteropAPI.CreateQuery(worldPtr, reqPtr, required.Length, optionalPtr, optional.Length, excludePtr, exclude.Length, iterPtr);
                            }
                        }
                        return;
                    }
                    else
                    {
                        fixed (QueryIterator* iterPtr = &iterator)
                        {
                            QueryInteropAPI.CreateQuery(worldPtr, reqPtr, required.Length, optionalPtr, optional.Length, null, 0, iterPtr);
                        }
                        return;
                    }
                }
            }

            if (exclude != null && exclude.Length > 0)
            {
                fixed (uint* ep = exclude)
                {
                    excludePtr = ep;
                    fixed (QueryIterator* iterPtr = &iterator)
                    {
                        QueryInteropAPI.CreateQuery(worldPtr, reqPtr, required.Length, null, 0, excludePtr, exclude.Length, iterPtr);
                    }
                }
                return;
            }

            // No optional/exclude
            fixed (QueryIterator* iterPtr = &iterator)
            {
                QueryInteropAPI.CreateQuery(worldPtr, reqPtr, required.Length, null, 0, null, 0, iterPtr);
            }
        }
    }
}
