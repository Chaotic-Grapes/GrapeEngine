/* Start Header *****************************************************************/
/*!
\file   Export.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Defines export macros for the Grape Engine, allowing for proper symbol visibility
when building and using the engine as a shared library (DLL). This header also 
includes macros for C API exports to facilitate interop with C# scripting. 
The GRAPEENGINE_API macro is used to annotate classes and functions that should 
be exported from the DLL, while the INTEROP_API macro is used for C-style 
functions that need to be accessible from other languages, such as C#.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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
