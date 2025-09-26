# ======================= Project Configuration =========================
cmake_minimum_required(VERSION 3.13)

# Set the CMake policy version (ensures consistent behavior)
cmake_policy(VERSION 3.13)

project(GrapeEngine)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Set the directory where custom CMake modules are stored
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Include project-wide utility and dependency modules
include(ImportDependencies)
importDependencies()

# FMOD root before importing
set(FMOD_ROOT "${CMAKE_CURRENT_LIST_DIR}/lib/Fmod" CACHE PATH "")

# Import FMOD interface targets
import_fmod()

# List of external libraries (updated for your dependencies)
set(ALL_LIBS
    PRIVATE glfw glad imgui glm FMOD::Core
        # FMOD::Studio
)

# ======================= SINGLE PROJECT CONFIGURATION =========================
# Since we have a single engine project, not using auto-discovery

# Collect your engine source files (preserving folder structure)
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c
)

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/include/*.h
    ${CMAKE_CURRENT_LIST_DIR}/include/*.hpp
)

# Create executable
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_library(GrapeEngine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
else()
    add_executable(GrapeEngine ${ENGINE_SOURCES} ${ENGINE_HEADERS})
endif()

target_compile_definitions(GrapeEngine PRIVATE USE_IMGUI)

# Preserve folder structure in Visual Studio
foreach(source IN LISTS ENGINE_SOURCES)
    get_filename_component(source_path "${source}" PATH)
    file(RELATIVE_PATH source_path_rel "${CMAKE_CURRENT_LIST_DIR}/src" "${source_path}")
    if(NOT source_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Source Files\\${source_path_rel}")
    else()
        set(group_path "Source Files")
    endif()
    source_group("${group_path}" FILES "${source}")
endforeach()

foreach(header IN LISTS ENGINE_HEADERS)
    get_filename_component(header_path "${header}" PATH)
    file(RELATIVE_PATH header_path_rel "${CMAKE_CURRENT_LIST_DIR}/include" "${header_path}")
    if(NOT header_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Header Files\\${header_path_rel}")
    else()
        set(group_path "Header Files")
    endif()
    source_group("${group_path}" FILES "${header}")
endforeach()

# Link libraries
target_link_libraries(GrapeEngine ${ALL_LIBS})

# Include directories
target_include_directories(GrapeEngine PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/include
)

if (TARGET GrapeEngine AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    fmod_copy_runtime(GrapeEngine)
endif()

# Set C++ standard for the target
set_property(TARGET GrapeEngine PROPERTY CXX_STANDARD 17)
set_property(TARGET GrapeEngine PROPERTY CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GrapeEngine PRIVATE -Wall -Wextra)
elseif (MSVC)
    target_compile_options(GrapeEngine PRIVATE /W3)
endif()

# Copy assets to build directory (if you have any)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
    add_custom_command(TARGET GrapeEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:GrapeEngine>/assets"
        COMMENT "Copying assets to build directory"
    )
endif()

message(STATUS "GrapeEngine configured successfully!")

# ======================= END CONFIGURATION =========================