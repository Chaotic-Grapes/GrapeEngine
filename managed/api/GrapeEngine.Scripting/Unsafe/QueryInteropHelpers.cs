using System;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

internal static unsafe class QueryInteropHelpers
{
    // Use small-stack allocations for optional/exclude arrays to avoid heap allocs
    // for the common small-case. Falls back to pinning the managed arrays.
    internal static void CreateQueryWithArrays(nuint worldPtr, uint[] required, uint[]? optional, uint[]? exclude, out QueryIterator iterator)
    {
        iterator = default;

        fixed (uint* reqPtr = required)
        {
            uint* optPtr = null;
            uint* excPtr = null;
            // stackalloc for small arrays
            const int StackThreshold = 8;

            uint* optStack = null;
            uint* excStack = null;

            if (optional != null && optional.Length > 0)
            {
                if (optional.Length <= StackThreshold)
                {
                    optStack = stackalloc uint[optional.Length];
                    for (int i = 0; i < optional.Length; ++i) optStack[i] = optional[i];
                    optPtr = optStack;
                }
            }

            if (exclude != null && exclude.Length > 0)
            {
                if (exclude.Length <= StackThreshold)
                {
                    excStack = stackalloc uint[exclude.Length];
                    for (int i = 0; i < exclude.Length; ++i) excStack[i] = exclude[i];
                    excPtr = excStack;
                }
            }

            // If we didn't use stackalloc for optional/exclude, pin the managed arrays
            if (optPtr == null && optional != null && optional.Length > 0)
            {
                fixed (uint* optionalPtr = optional)
                {
                    if (excPtr == null && exclude != null && exclude.Length > 0)
                    {
                        fixed (uint* excludePtr = exclude)
                        {
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
                            QueryInteropAPI.CreateQuery(worldPtr, reqPtr, required.Length, optionalPtr, optional.Length, excPtr, exclude?.Length ?? 0, iterPtr);
                        }
                        return;
                    }
                }
            }

            if (excPtr == null && exclude != null && exclude.Length > 0)
            {
                fixed (uint* excludePtr = exclude)
                {
                    fixed (QueryIterator* iterPtr = &iterator)
                    {
                        QueryInteropAPI.CreateQuery(worldPtr, reqPtr, required.Length, optPtr, optional?.Length ?? 0, excludePtr, exclude.Length, iterPtr);
                    }
                }
                return;
            }

            // Both optional/exclude used stackalloc or are null
            fixed (QueryIterator* iterPtr = &iterator)
            {
                QueryInteropAPI.CreateQuery(worldPtr, reqPtr, required.Length, optPtr, optional?.Length ?? 0, excPtr, exclude?.Length ?? 0, iterPtr);
            }
        }
    }
}
