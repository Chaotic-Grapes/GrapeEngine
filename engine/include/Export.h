#pragma once

// Suppress warnings about STL types in DLL interface
// C4251: class needs to have dll-interface to be used by clients
// This is safe when both DLL and client use the same compiler/runtime
#pragma warning(disable: 4251)

#if defined(_WIN32) || defined(_WIN64)
#  ifdef GRAPEENGINE_EXPORTS
#    define GRAPEENGINE_API __declspec(dllexport)
#  else
#    define GRAPEENGINE_API __declspec(dllimport)
#  endif
#else
#  define GRAPEENGINE_API
#endif

// C API exports for C# scripting interop
#if defined(_WIN32) || defined(_WIN64)
    #ifdef BUILDING_INTEROP
        #define INTEROP_API extern "C" __declspec(dllexport)
    #else
        #define INTEROP_API extern "C" __declspec(dllimport)
    #endif
#else
    #define INTEROP_API extern "C"
#endif
