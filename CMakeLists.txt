# ======================= Project Configuration =========================
cmake_minimum_required(VERSION 3.13)

# Set the CMake policy version (ensures consistent behavior)
cmake_policy(VERSION 3.13)

project(GrapeEngine)

# To make compile time faster
# Enable /MP for all targets globally
if(MSVC)
    add_compile_options(/MP)
endif()

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the directory where custom CMake modules are stored
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Include project-wide utility and dependency modules
include(ImportDependencies)
importDependencies()

# Find .NET SDK
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

# ======================= CoreCLR / .NET Hosting Configuration =================
# Set the path to the CoreCLR library directory
set(CORECLR_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/lib/CoreCLR")

# Find nethost library
find_library(NETHOST_LIB 
    NAMES nethost
    PATHS ${CORECLR_LIB_DIR}
    NO_DEFAULT_PATH
    REQUIRED
)

if(NETHOST_LIB)
    message(STATUS "Found nethost library: ${NETHOST_LIB}")
else()
    message(FATAL_ERROR "nethost.lib not found in ${CORECLR_LIB_DIR}")
    message(FATAL_ERROR "Please place nethost.lib in lib/CoreCLR/")
endif()

# Verify nethost.dll exists for runtime
if(EXISTS "${CORECLR_LIB_DIR}/nethost.dll")
    message(STATUS "Found nethost.dll: ${CORECLR_LIB_DIR}/nethost.dll")
else()
    message(WARNING "nethost.dll not found in ${CORECLR_LIB_DIR}")
    message(WARNING "Runtime may fail - please place nethost.dll in lib/CoreCLR/")
endif()

# ======================= SINGLE PROJECT CONFIGURATION =========================
# Since we have a single engine project, not using auto-discovery

# Collect your engine source files (preserving folder structure)
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/ImGuizmo/*.cpp
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
set_property(TARGET GrapeEngine PROPERTY CXX_STANDARD 23)
set_property(TARGET GrapeEngine PROPERTY CXX_STANDARD_REQUIRED ON)

# Compile definitions
target_compile_definitions(GrapeEngine PRIVATE USE_IMGUI)

# Include directories
target_include_directories(GrapeEngine PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/include/engine
    ${CMAKE_CURRENT_LIST_DIR}/include/editor
    ${CMAKE_CURRENT_LIST_DIR}/include/externals
    ${CMAKE_CURRENT_LIST_DIR}/ImGuizmo
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
        ${NETHOST_LIB}
)

# Export functions from executable for C# P/Invoke
# Note: This is temporary until we refactor to a DLL!
if(MSVC)
    set_target_properties(GrapeEngine PROPERTIES
        LINK_FLAGS "/EXPORT:ScriptAPI_GetComponent /EXPORT:ScriptAPI_SetComponent /EXPORT:ScriptAPI_HasComponent /EXPORT:ScriptAPI_RemoveComponent /EXPORT:ScriptAPI_DestroyEntity /EXPORT:ScriptAPI_SetWorld"
    )
endif()

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
        "$<TARGET_FILE_DIR:GrapeEngine>"
    COMMENT "Copying FMOD DLL to output directory"
)

# Copy nethost.dll for CoreCLR hosting (Windows)
if(EXISTS "${CORECLR_LIB_DIR}/nethost.dll")
    add_custom_command(TARGET GrapeEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CORECLR_LIB_DIR}/nethost.dll"
            "$<TARGET_FILE_DIR:GrapeEngine>"
        COMMENT "Copying nethost.dll to output directory"
    )
endif()

# ======================= C# Scripting API Build ===========================
# Build the engine's scripting API AND any game scripts in scripts/ folder
# This allows game scripts to be separate C# projects while being built automatically
if(DOTNET_EXECUTABLE)
    message(STATUS "Configuring C# Scripting Projects...")
    
    # Build GrapeEngine.ScriptAPI (engine's API)
    add_custom_target(
        BuildScriptAPI ALL
        COMMAND ${DOTNET_EXECUTABLE} build "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI/GrapeEngine.ScriptAPI.csproj" 
            --configuration $<IF:$<CONFIG:Debug>,Debug,Release>
            --output "${CMAKE_BINARY_DIR}/$<CONFIG>" # Output to same directory as .exe file
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/scripts"
        COMMENT "Building GrapeEngine Scripting API..."
        VERBATIM
    )
    
    # Find all .csproj files in scripts/ folder (excluding the API itself)
    file(GLOB GAME_SCRIPT_PROJECTS 
        "${CMAKE_CURRENT_LIST_DIR}/scripts/*/*.csproj"
    )
    
    # Build each game script project found
    set(GAME_SCRIPT_TARGETS "")
    foreach(CSPROJ_FILE ${GAME_SCRIPT_PROJECTS})
        # Skip the ScriptAPI project (already built above)
        get_filename_component(PROJ_NAME ${CSPROJ_FILE} NAME_WE)
        if(NOT PROJ_NAME STREQUAL "GrapeEngine.ScriptAPI")
            message(STATUS "  Found game script project: ${PROJ_NAME}")
            
            # Create a build target for this project
            add_custom_target(
                Build_${PROJ_NAME} ALL
                COMMAND ${DOTNET_EXECUTABLE} build "${CSPROJ_FILE}"
                    --configuration $<IF:$<CONFIG:Debug>,Debug,Release>
                    --output "${CMAKE_BINARY_DIR}/$<CONFIG>" # Output to same directory as .exe file
                WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/scripts"
                COMMENT "Building ${PROJ_NAME}..."
                DEPENDS BuildScriptAPI
                VERBATIM
            )
            
            list(APPEND GAME_SCRIPT_TARGETS Build_${PROJ_NAME})
        endif()
    endforeach()
    
    # Make GrapeEngine depend on all script builds
    add_dependencies(GrapeEngine BuildScriptAPI ${GAME_SCRIPT_TARGETS})
    
    # Note: C# DLLs are already built directly to ${CMAKE_BINARY_DIR}/$<CONFIG>
    # which is the same directory as the executable, so no copying is needed.

    # Copy runtimeconfig.json for .NET hosting
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI.runtimeconfig.json")
        add_custom_command(TARGET GrapeEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI.runtimeconfig.json"
                "$<TARGET_FILE_DIR:GrapeEngine>"
            COMMENT "Copying runtimeconfig.json to output directory"
        )
    endif()
    
    message(STATUS "C# build output: ${CMAKE_BINARY_DIR}/$<CONFIG>")
    message(STATUS "DLLs will be copied to: $<TARGET_FILE_DIR:GrapeEngine>")
    #message(STATUS "Game scripts can be added as separate .csproj files in scripts/ folder")
else()
    message(WARNING ".NET SDK not found - Scripting will not be available")
endif()

# ======================= Game Project Copy =========================
# TODO: Remove this section when editor is separated from engine
# Copy GameProject folder to build directory so editor can access game assets
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/EchoesBelow")
    add_custom_command(TARGET GrapeEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/EchoesBelow"
        "$<TARGET_FILE_DIR:GrapeEngine>/EchoesBelow"
        COMMENT "Copying EchoesBelow to build directory"
    )
    message(STATUS "The game project 'EchoesBelow' will be copied to build directory")
endif()

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