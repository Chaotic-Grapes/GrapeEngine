using System;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

internal static unsafe class QueryInteropHelpers
{
    // Centralized CreateQuery helper that pins managed arrays and calls the native API.
    internal static void CreateQueryWithArrays(void* worldPtr, uint[] required, uint[]? optional, uint[]? exclude, out QueryIterator iterator)
    {
        iterator = default;

        // required is always present; keep it pinned for the entire call path.
        fixed (uint* reqPtr = required)
        {
            uint* optionalPtr = null;
            uint* excludePtr = null;

            // Pin optional and exclude arrays together when both exist so the native call
            // can consume stable pointers in a single invocation.
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
                        // Optional-only query.
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
                // Exclude-only query.
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


