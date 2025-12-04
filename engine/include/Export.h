#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  ifdef GRAPEENGINE_EXPORTS
#    define GRAPEENGINE_API __declspec(dllexport)
#  else
#    define GRAPEENGINE_API __declspec(dllimport)
#  endif
#else
#  define GRAPEENGINE_API
#endif
