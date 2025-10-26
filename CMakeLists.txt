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

# Create executable or library
# Right now, both should be .exe
add_executable(GrapeEngine ${ENGINE_SOURCES} ${ENGINE_HEADERS})
#if(CMAKE_BUILD_TYPE STREQUAL "Release")
#    add_library(GrapeEngine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
#else()
#    add_executable(GrapeEngine ${ENGINE_SOURCES} ${ENGINE_HEADERS})
#endif()

# Set C++ standard for the target
set_property(TARGET GrapeEngine PROPERTY CXX_STANDARD 17)
set_property(TARGET GrapeEngine PROPERTY CXX_STANDARD_REQUIRED ON)

# Compile definitions
target_compile_definitions(GrapeEngine PRIVATE USE_IMGUI)

# Include directories
target_include_directories(GrapeEngine PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/include/engine
    ${CMAKE_CURRENT_LIST_DIR}/include/test
    ${CMAKE_CURRENT_LIST_DIR}/include/externals
)

# Link libraries
target_link_libraries(GrapeEngine
    PRIVATE
        glfw
        glad
        imgui
        glm
        freetype
        fmod
)

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

# Compiler warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GrapeEngine PRIVATE -Wall -Wextra -Wpedantic -Werror)
elseif (MSVC)
    target_compile_options(GrapeEngine PRIVATE /W4)
endif()

# ======================= Copy files and assets =========================

# Copy assets to build directory
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
    add_custom_command(TARGET GrapeEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:GrapeEngine>/assets"
        COMMENT "Copying assets to build directory"
    )
    add_custom_command(TARGET GrapeEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "${CMAKE_BINARY_DIR}/assets"
        COMMENT "Copying assets to project folder"
    )
endif()

# Copy FMOD DLL (Windows)
add_custom_command(TARGET GrapeEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_LIST_DIR}/lib/fmod/fmod.dll"
        $<TARGET_FILE_DIR:GrapeEngine>
    COMMENT "Copying FMOD DLL to output directory"
)

message(STATUS "GrapeEngine configured successfully!")

# ======================= DOCUMENTATION GENERATION =========================
find_package(Doxygen OPTIONAL_COMPONENTS dot)

if(NOT DOXYGEN_FOUND)
    message(WARNING "Doxygen is not installed - documentation generation disabled")
    message(STATUS "Install from: https://www.doxygen.nl/download.html")
else()
    message(STATUS "Doxygen located at: ${DOXYGEN_EXECUTABLE}")

    # Define documentation paths
    set(DOC_INPUT_PATH "${CMAKE_CURRENT_LIST_DIR}/include")
    set(DOC_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/src")
    set(DOC_BUILD_PATH "${CMAKE_BINARY_DIR}/Documentation")

    # Setup Graphviz if available
    if(DOXYGEN_DOT_FOUND)
        set(HAVE_DOT "YES")
        set(DOT_EXECUTABLE_PATH ${DOXYGEN_DOT_EXECUTABLE})
        message(STATUS "Graphviz found - class diagrams will be generated")
    else()
        set(HAVE_DOT "NO")
        set(DOT_EXECUTABLE_PATH "")
        message(STATUS "Graphviz not found - diagrams disabled (optional)")
    endif()

    # Generate Doxygen configuration from template
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/Doxyfile.in"
        "${CMAKE_BINARY_DIR}/Doxyfile"
        @ONLY
    )

    # Manual documentation build target
    add_custom_target(
        generate_docs
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building API documentation..."
        VERBATIM
    )

    # Convenience target to view generated docs (cross-platform)
    if(WIN32)
        add_custom_target(
            view_docs
            COMMAND cmd /c start "" "${DOC_BUILD_PATH}/HTML/index.html"
            DEPENDS generate_docs
            COMMENT "Launching documentation in browser..."
        )
    elseif(APPLE)
        add_custom_target(
            view_docs
            COMMAND open "${DOC_BUILD_PATH}/HTML/index.html"
            DEPENDS generate_docs
            COMMENT "Launching documentation in browser..."
        )
    elseif(UNIX)
        add_custom_target(
            view_docs
            COMMAND xdg-open "${DOC_BUILD_PATH}/HTML/index.html"
            DEPENDS generate_docs
            COMMENT "Launching documentation in browser..."
        )
    endif()

    # Track source files for incremental rebuilds
    file(GLOB_RECURSE DOC_DEPENDENCIES CONFIGURE_DEPENDS
        "${DOC_INPUT_PATH}/*.h"
        "${DOC_INPUT_PATH}/*.hpp"
        "${DOC_SOURCE_PATH}/*.cpp"
        "${DOC_SOURCE_PATH}/*.c"
        "${CMAKE_CURRENT_LIST_DIR}/*.md"
    )

    # Automatic incremental documentation generation
    add_custom_command(
        OUTPUT "${DOC_BUILD_PATH}/.last_build"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DOC_BUILD_PATH}"
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/Doxyfile"
        COMMAND ${CMAKE_COMMAND} -E touch "${DOC_BUILD_PATH}/.last_build"
        DEPENDS ${DOC_DEPENDENCIES} "${CMAKE_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Updating documentation automatically..."
        VERBATIM
    )

    # Auto-rebuild target
    add_custom_target(
        auto_docs ALL
        DEPENDS "${DOC_BUILD_PATH}/.last_build"
    )

    message(STATUS "Doxygen documentation configured")
    message(STATUS "  Manual build: cmake --build . --target generate_docs")
    message(STATUS "  Build & view: cmake --build . --target view_docs")
    message(STATUS "  Auto-rebuild: enabled (runs on every build)")
    message(STATUS "  Output: ${DOC_BUILD_PATH}/HTML/index.html")
endif()

# ======================= END DOCUMENTATION GENERATION ===================