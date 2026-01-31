# ======================= Engine Library Configuration =========================
# Engine is a standalone C++ library containing the core ECS, rendering, physics,
# and systems. It has NO dependencies on Editor code or ImGui.
#
# This library can be:
# - Linked by the Editor executable
# - Linked by the Runtime/Game executable
# - Wrapped by Interop for C# scripting
# ===============================================================================

cmake_minimum_required(VERSION 3.13)

project(GrapeEngineLib VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ======================= Collect Engine Source Files =========================
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c
)

# GlobalMemoryOverride.cpp is already picked up by GLOB_RECURSE above

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/include/*.h
    ${CMAKE_CURRENT_LIST_DIR}/include/*.hpp
)

# ======================= External Dependencies =========================
# Include external source files that are part of the core engine
file(GLOB_RECURSE EXTERNAL_SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/../externals/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../externals/src/*.c
)

file(GLOB_RECURSE EXTERNAL_HEADERS CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/../externals/include/*.h
    ${CMAKE_CURRENT_LIST_DIR}/../externals/include/*.hpp
)

# Exclude editor-only files from externals (ImGuizmo, UICommon)
list(FILTER EXTERNAL_SOURCES EXCLUDE REGEX ".*/ImGuizmo\\.cpp$")
list(FILTER EXTERNAL_SOURCES EXCLUDE REGEX ".*/UICommon\\.cpp$")
list(FILTER EXTERNAL_HEADERS EXCLUDE REGEX ".*/UICommon\\.h$")

# ======================= Create Engine Library =========================
# Option: build as shared (DLL) or static library
option(BUILD_ENGINE_SHARED "Build engine as shared library (DLL)" ON)

if(BUILD_ENGINE_SHARED)
    add_library(GrapeEngineLib SHARED 
        ${ENGINE_SOURCES} 
        ${ENGINE_HEADERS}
        ${EXTERNAL_SOURCES}
        ${EXTERNAL_HEADERS}
    )
    message(STATUS "Engine: Building as SHARED library (DLL)")
    
    # Define export macros for shared library
    target_compile_definitions(GrapeEngineLib PRIVATE 
        GRAPEENGINE_EXPORTS
        BUILDING_INTEROP
    )
    
    # Set stable output name for managed interop
    set_target_properties(GrapeEngineLib PROPERTIES OUTPUT_NAME "GrapeEngineNative")
else()
    add_library(GrapeEngineLib STATIC 
        ${ENGINE_SOURCES} 
        ${ENGINE_HEADERS}
        ${EXTERNAL_SOURCES}
        ${EXTERNAL_HEADERS}
    )
    message(STATUS "Engine: Building as STATIC library")
endif()

# ======================= Include Directories =========================
target_include_directories(GrapeEngineLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../externals/include
)

# ======================= Link Dependencies =========================
# CoreCLR / .NET Hosting
set(CORECLR_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../lib/CoreCLR")
find_library(NETHOST_LIB 
    NAMES nethost
    PATHS ${CORECLR_LIB_DIR}
    NO_DEFAULT_PATH
    REQUIRED
)

if(NETHOST_LIB)
    message(STATUS "Engine: Found nethost library: ${NETHOST_LIB}")
else()
    message(FATAL_ERROR "Engine: nethost.lib not found in ${CORECLR_LIB_DIR}")
endif()

# Link core dependencies (NO ImGui - that's editor-only)
target_link_libraries(GrapeEngineLib
    PUBLIC
        glfw
        glad
        glm
        freetype
        ${NETHOST_LIB}
)

if(TARGET fmod)
    target_link_libraries(GrapeEngineLib
        PUBLIC
            fmod
    )
else()
    message(WARNING "Engine: FMOD target not available; audio features disabled unless FMOD is provided in externals/lib/Fmod or lib/fmod")
endif()

# ======================= Compiler Definitions =========================
# Engine does NOT define USE_IMGUI - editor code is completely separate
# Interop exports are handled via __declspec(dllexport) when BUILDING_INTEROP is defined

# ======================= IDE Organization - Visual Studio Filters =========================
# Organize engine source files by folder structure
foreach(source IN LISTS ENGINE_SOURCES)
    get_filename_component(source_path "${source}" PATH)
    file(RELATIVE_PATH source_path_rel "${CMAKE_CURRENT_LIST_DIR}/src" "${source_path}")
    if(NOT source_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Engine Source\\${source_path_rel}")
    else()
        set(group_path "Engine Source")
    endif()
    source_group("${group_path}" FILES "${source}")
endforeach()

# Organize engine header files by folder structure
foreach(header IN LISTS ENGINE_HEADERS)
    get_filename_component(header_path "${header}" PATH)
    file(RELATIVE_PATH header_path_rel "${CMAKE_CURRENT_LIST_DIR}/include" "${header_path}")
    if(NOT header_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Engine Include\\${header_path_rel}")
    else()
        set(group_path "Engine Include")
    endif()
    source_group("${group_path}" FILES "${header}")
endforeach()

# Organize external source files by folder structure
foreach(source IN LISTS EXTERNAL_SOURCES)
    get_filename_component(source_path "${source}" PATH)
    file(RELATIVE_PATH source_path_rel "${CMAKE_CURRENT_LIST_DIR}/../externals/src" "${source_path}")
    if(NOT source_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Externals\\${source_path_rel}")
    else()
        set(group_path "Externals")
    endif()
    source_group("${group_path}" FILES "${source}")
endforeach()

# Organize external header files by folder structure
foreach(header IN LISTS EXTERNAL_HEADERS)
    get_filename_component(header_path "${header}" PATH)
    file(RELATIVE_PATH header_path_rel "${CMAKE_CURRENT_LIST_DIR}/../externals/include" "${header_path}")
    if(NOT header_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Externals\\${header_path_rel}")
    else()
        set(group_path "Externals")
    endif()
    source_group("${group_path}" FILES "${header}")
endforeach()

# ======================= Installation (Optional) =========================
# This allows engine to be installed as a reusable library
install(TARGETS GrapeEngineLib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
    DESTINATION include
)

message(STATUS "Engine: Configuration complete")
message(STATUS "  Sources: ${CMAKE_CURRENT_LIST_DIR}/src")
message(STATUS "  Headers: ${CMAKE_CURRENT_LIST_DIR}/include")
message(STATUS "  Type: $<IF:$<BOOL:${BUILD_ENGINE_SHARED}>,SHARED,STATIC>")
