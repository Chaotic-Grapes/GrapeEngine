# ======================= Project Configuration =========================
cmake_minimum_required(VERSION 3.13)

# Set the CMake policy version (ensures consistent behavior)
cmake_policy(VERSION 3.13)

project(GrapeEngine)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the directory where custom CMake modules are stored
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Include project-wide utility and dependency modules
include(ImportDependencies)
importDependencies()

# Prevent ImGui (and any editor-only UI libs) from being built into the
# standalone game output directory. If an `imgui` target exists, mark it
# excluded when `BUILD_GAME` is enabled so it won't produce `imgui.lib`/`imgui.pdb`
# in game-only builds.
if(BUILD_GAME)
    if(TARGET imgui)
        set_target_properties(imgui PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endif()

# Find .NET SDK
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

# ================ CoreCLR / .NET Hosting Configuration =================
# Set the path to the CoreCLR library directory (primary default)
set(CORECLR_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/lib/CoreCLR")

# Also consider common fallback locations and an environment override
set(CORECLR_FALLBACK_DIR "${CMAKE_CURRENT_LIST_DIR}/externals/lib/CoreCLR")
set(NETHOST_ENV_DIR $ENV{NETHOST_DIR})

# Build a search list for find_library so CMake can locate nethost in a few places
set(NETHOST_SEARCH_DIRS
    ${CORECLR_LIB_DIR}
    ${CORECLR_FALLBACK_DIR}
    ${NETHOST_ENV_DIR}
)

# Find nethost library (allow system defaults as a last resort)
find_library(NETHOST_LIB
    NAMES nethost
    PATHS ${NETHOST_SEARCH_DIRS}
    REQUIRED
)

if(NETHOST_LIB)
    message(STATUS "Found nethost library: ${NETHOST_LIB}")
    get_filename_component(NETHOST_LIB_DIR ${NETHOST_LIB} DIRECTORY)
    message(STATUS "nethost library directory: ${NETHOST_LIB_DIR}")
else()
    message(FATAL_ERROR "nethost.lib not found. Searched: ${NETHOST_SEARCH_DIRS}")
    message(FATAL_ERROR "Please place nethost.lib in one of the above locations or set the NETHOST_DIR environment variable.")
endif()

# Verify nethost.dll exists for runtime (use the directory where the library was found)
if(DEFINED NETHOST_LIB_DIR AND EXISTS "${NETHOST_LIB_DIR}/nethost.dll")
    message(STATUS "Found nethost.dll: ${NETHOST_LIB_DIR}/nethost.dll")
else()
    message(WARNING "nethost.dll not found in ${NETHOST_LIB_DIR}")
    message(WARNING "Runtime may fail - please place nethost.dll alongside nethost.lib or set NETHOST_DIR env var to its folder.")
endif()

# ======================= BUILD CONFIGURATION =========================
# Three-project architecture: Engine (lib) -> Editor (exe) & Runtime (exe)
option(BUILD_EDITOR "Build the level editor (includes ImGui)" ON)
option(BUILD_GAME "Build the standalone game runtime (no editor, no ImGui)" OFF)
option(BUILD_ENGINE_SHARED "Build engine as shared library (DLL)" ON)

if(NOT BUILD_EDITOR AND NOT BUILD_GAME)
    message(FATAL_ERROR "At least one of BUILD_EDITOR or BUILD_GAME must be enabled")
endif()

message(STATUS "========================================")
message(STATUS "Build Configuration:")
message(STATUS "  Engine Library: ${CMAKE_CURRENT_LIST_DIR}/engine")
message(STATUS "  Engine Type: $<IF:$<BOOL:${BUILD_ENGINE_SHARED}>,SHARED (DLL),STATIC>")
message(STATUS "  BUILD_EDITOR: ${BUILD_EDITOR}")
message(STATUS "  BUILD_GAME: ${BUILD_GAME}")
message(STATUS "========================================")

# ---------------------- Icon / Resource configuration --------------------
# Default icon (can be overridden via CMake cache):
set(GAME_ICON "${CMAKE_CURRENT_LIST_DIR}/assets/game_icon.ico" CACHE FILEPATH "Icon file for the standalone game executable")
set(EDITOR_ICON "${CMAKE_CURRENT_LIST_DIR}/assets/editor_icon.ico" CACHE FILEPATH "Icon file for the editor executable")

# Generate per-target .rc files from templates so each target can embed a different icon.
configure_file("${CMAKE_CURRENT_LIST_DIR}/app_game.rc.in" "${CMAKE_BINARY_DIR}/app_game.rc" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/app_editor.rc.in" "${CMAKE_BINARY_DIR}/app_editor.rc" @ONLY)

if(NOT EXISTS "${GAME_ICON}")
    message(WARNING "Game icon not found: ${GAME_ICON} — the resource compile may fail")
endif()
if(NOT EXISTS "${EDITOR_ICON}")
    message(WARNING "Editor icon not found: ${EDITOR_ICON} — the resource compile may fail")
endif()

# ======================= BUILD SUBDIRECTORIES =========================
# Build order: Engine (library) → Editor/Runtime (executables)

# Always build the engine library first
add_subdirectory(engine)
# Build editor if requested
if(BUILD_EDITOR)
    add_subdirectory(editor)
    set(PRIMARY_TARGET GrapeEditor)
endif()

# Build runtime/game if requested
if(BUILD_GAME)
    add_subdirectory(runtime)
    if(NOT BUILD_EDITOR)
        set(PRIMARY_TARGET GrapeRuntime)
    endif()
endif()

# ======================= SCRIPT API EXPORTS =========================
# C# P/Invoke exports are now handled in subdirectory CMakeLists
# - engine/CMakeLists.txt handles exports for shared library builds
# - editor/CMakeLists.txt handles exports when engine is static
# - runtime/CMakeLists.txt handles exports when engine is static

# ======================= IDE ORGANIZATION & WARNINGS =========================
# Preserve folder structure in Visual Studio for both targets
if(BUILD_EDITOR)
    foreach(source IN LISTS ENGINE_SOURCES EDITOR_SOURCES)
        get_filename_component(source_path "${source}" PATH)
        file(RELATIVE_PATH source_path_rel "${CMAKE_CURRENT_LIST_DIR}/src" "${source_path}")
        if(NOT source_path_rel STREQUAL "")
            string(REPLACE "/" "\\" group_path "Source Files\\${source_path_rel}")
        else()
            set(group_path "Source Files")
        endif()
        source_group("${group_path}" FILES "${source}")
    endforeach()

    foreach(header IN LISTS ENGINE_HEADERS EDITOR_HEADERS)
        get_filename_component(header_path "${header}" PATH)
        file(RELATIVE_PATH header_path_rel "${CMAKE_CURRENT_LIST_DIR}/include" "${header_path}")
        if(NOT header_path_rel STREQUAL "")
            string(REPLACE "/" "\\" group_path "Header Files\\${header_path_rel}")
        else()
            set(group_path "Header Files")
        endif()
        source_group("${group_path}" FILES "${header}")
    endforeach()
    
    # Compiler warnings for editor: apply to the actual editor or engine targets
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(TARGET GrapeEditor)
            target_compile_options(GrapeEditor PRIVATE -Wall -Wextra -Wpedantic -Werror)
        elseif(TARGET GrapeEngineLib)
            target_compile_options(GrapeEngineLib PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
    elseif (MSVC)
        if(TARGET GrapeEditor)
            target_compile_options(GrapeEditor PRIVATE /W4)
        elseif(TARGET GrapeEngineLib)
            target_compile_options(GrapeEngineLib PRIVATE /W4)
        endif()
    endif()
endif()

if(BUILD_GAME)
    foreach(source IN LISTS ENGINE_SOURCES GAME_SOURCES)
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
    
    # Compiler warnings for game: apply to runtime or engine targets
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(TARGET GrapeRuntime)
            target_compile_options(GrapeRuntime PRIVATE -Wall -Wextra -Wpedantic -Werror)
        elseif(TARGET GrapeEngineLib)
            target_compile_options(GrapeEngineLib PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
    elseif (MSVC)
        if(TARGET GrapeRuntime)
            target_compile_options(GrapeRuntime PRIVATE /W4)
        elseif(TARGET GrapeEngineLib)
            target_compile_options(GrapeEngineLib PRIVATE /W4)
        endif()
    endif()
endif()

# ======================= Copy files and assets =========================

# Copy assets to build directory for editor
if(BUILD_EDITOR AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
    if(TARGET GrapeEditor)
        add_custom_target(Copy_Assets_Editor ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_LIST_DIR}/assets"
                "$<TARGET_FILE_DIR:GrapeEditor>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_LIST_DIR}/assets"
                "${CMAKE_BINARY_DIR}/assets"
            DEPENDS GrapeEditor
            COMMENT "Copying assets to editor build directory and project folder"
        )
    endif()
endif()

# Copy assets to build directory for game
if(BUILD_GAME AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
    if(TARGET GrapeRuntime)
        add_custom_target(Copy_Assets_Runtime ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_LIST_DIR}/assets"
                "$<TARGET_FILE_DIR:GrapeRuntime>/assets"
            DEPENDS GrapeRuntime
            COMMENT "Copying assets to game build directory"
        )
    endif()
endif()

# Copy FMOD DLL (Windows) for editor
if(BUILD_EDITOR)
    if(DEFINED FMOD_LIB_DIR AND EXISTS "${FMOD_LIB_DIR}/fmod.dll")
        if(TARGET GrapeEditor)
            add_custom_target(Copy_FMOD_Editor ALL
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FMOD_LIB_DIR}/fmod.dll"
                    "$<TARGET_FILE_DIR:GrapeEditor>"
                DEPENDS GrapeEditor
                COMMENT "Copying FMOD DLL to editor output directory"
            )
        endif()
    else()
        message(STATUS "FMOD DLL not found; skipping FMOD copy for editor. Searched: ${FMOD_LIB_DIR}/fmod.dll")
    endif()
endif()

# Copy FMOD DLL (Windows) for game
if(BUILD_GAME)
    if(DEFINED FMOD_LIB_DIR AND EXISTS "${FMOD_LIB_DIR}/fmod.dll")
        if(TARGET GrapeRuntime)
            add_custom_target(Copy_FMOD_Runtime ALL
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FMOD_LIB_DIR}/fmod.dll"
                    "$<TARGET_FILE_DIR:GrapeRuntime>"
                DEPENDS GrapeRuntime
                COMMENT "Copying FMOD DLL to game output directory"
            )
        endif()
    else()
        message(STATUS "FMOD DLL not found; skipping FMOD copy for game. Searched: ${FMOD_LIB_DIR}/fmod.dll")
    endif()
endif()

# Copy nethost.dll for CoreCLR hosting (Windows) for editor
if(BUILD_EDITOR AND DEFINED NETHOST_LIB_DIR AND EXISTS "${NETHOST_LIB_DIR}/nethost.dll")
    if(TARGET GrapeEditor)
        add_custom_target(Copy_Nethost_Editor ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${NETHOST_LIB_DIR}/nethost.dll"
                "$<TARGET_FILE_DIR:GrapeEditor>"
            DEPENDS GrapeEditor
            COMMENT "Copying nethost.dll to editor output directory"
        )
    endif()
endif()

# Copy nethost.dll for CoreCLR hosting (Windows) for game
if(BUILD_GAME AND DEFINED NETHOST_LIB_DIR AND EXISTS "${NETHOST_LIB_DIR}/nethost.dll")
    if(TARGET GrapeRuntime)
        add_custom_target(Copy_Nethost_Runtime ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${NETHOST_LIB_DIR}/nethost.dll"
                "$<TARGET_FILE_DIR:GrapeRuntime>"
            DEPENDS GrapeRuntime
            COMMENT "Copying nethost.dll to game output directory"
        )
    endif()
endif()

# ======================= C# Scripting API Build ===========================
# Build the engine's scripting API AND discover ALL .cs files in game projects
if(DOTNET_EXECUTABLE)
    message(STATUS "Configuring C# Scripting Projects...")
    
    # Detect installed .NET SDK version dynamically
    execute_process(
        COMMAND ${DOTNET_EXECUTABLE} --list-sdks
        OUTPUT_VARIABLE DOTNET_SDK_LIST
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # Find all .NET SDK versions and select the highest one
    string(REGEX MATCHALL "([0-9]+\\.[0-9]+\\.[0-9]+)" DOTNET_SDK_VERSIONS "${DOTNET_SDK_LIST}")
    
    # Sort versions and pick the highest (last in sorted list)
    list(SORT DOTNET_SDK_VERSIONS COMPARE NATURAL ORDER DESCENDING)
    list(GET DOTNET_SDK_VERSIONS 0 DOTNET_SDK_VERSION)
    
    if(DOTNET_SDK_VERSION)
        message(STATUS "  Detected .NET SDK version: ${DOTNET_SDK_VERSION}")
        
        # Determine target framework moniker (TFM) for generated projects
        string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" DOTNET_MAJOR_MINOR "${DOTNET_SDK_VERSION}")
        set(DOTNET_TFM "net${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
        message(STATUS "  Target framework: ${DOTNET_TFM}")
    else()
        message(WARNING "  Could not detect .NET SDK version")
        set(DOTNET_TFM "net9.0")  # Fallback
    endif()
    
    # Build GrapeEngine.Scripting (managed API and CLR hosting)
    add_custom_target(
        BuildScripting ALL
        COMMAND ${DOTNET_EXECUTABLE} restore "${CMAKE_CURRENT_LIST_DIR}/managed/api/GrapeEngine.Scripting/GrapeEngine.Scripting.csproj"
        COMMAND ${DOTNET_EXECUTABLE} build "${CMAKE_CURRENT_LIST_DIR}/managed/api/GrapeEngine.Scripting/GrapeEngine.Scripting.csproj" 
            --configuration $<IF:$<CONFIG:Debug>,Debug,Release>
            --output "${CMAKE_BINARY_DIR}/$<CONFIG>"
            --no-restore
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/managed/api"
        COMMENT "Building GrapeEngine.Scripting (managed API and CLR hosting)..."
        VERBATIM
    )
    
    # Make targets depend on managed assemblies
    if(BUILD_EDITOR)
        add_dependencies(GrapeEditor BuildScripting)
    endif()
    if(BUILD_GAME)
        add_dependencies(GrapeRuntime BuildScripting)
    endif()
    
    # NOTE: User game scripts (e.g., EchoesBelow/*.cs) are NOT compiled by CMake!
    # They will be compiled at runtime by the Roslyn compiler integrated into the engine/editor.
    # This is the new architecture - scripts are plain .cs files with no .csproj needed.
    
    # Note: C# DLLs are already built directly to ${CMAKE_BINARY_DIR}/$<CONFIG>
    # which is the same directory as the executable, so no copying is needed.

    # Copy runtimeconfig.json for .NET hosting (editor)
    if(BUILD_EDITOR AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/managed/api/GrapeEngine.Scripting.runtimeconfig.json")
        if(TARGET GrapeEditor)
            add_custom_target(Copy_RuntimeConfig_Editor ALL
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_LIST_DIR}/managed/api/GrapeEngine.Scripting.runtimeconfig.json"
                    "$<TARGET_FILE_DIR:GrapeEditor>"
                DEPENDS GrapeEditor
                COMMENT "Copying .NET runtimeconfig.json to editor output directory"
            )
        endif()
    endif()

    # Copy runtimeconfig.json for .NET hosting (game)
    if(BUILD_GAME AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/managed/api/GrapeEngine.Scripting.runtimeconfig.json")
        if(TARGET GrapeRuntime)
            add_custom_target(Copy_RuntimeConfig_Runtime ALL
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_LIST_DIR}/managed/api/GrapeEngine.Scripting.runtimeconfig.json"
                    "$<TARGET_FILE_DIR:GrapeRuntime>"
                DEPENDS GrapeRuntime
                COMMENT "Copying .NET runtimeconfig.json to game output directory"
            )
        endif()
    endif()
    
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "C# Scripting Configuration:")
    message(STATUS "  Assembly: GrapeEngine.Scripting.dll")
    message(STATUS "  Output: ${CMAKE_BINARY_DIR}/$<CONFIG>")
    message(STATUS "")
    message(STATUS "User Script Development:")
    message(STATUS "  - Write .cs scripts in game project folders (e.g., EchoesBelow/Scripts/)")
    message(STATUS "  - NO .csproj needed - scripts are plain .cs files")
    message(STATUS "  - Compiled at runtime via Roslyn in engine/editor")
    message(STATUS "  - Hot reload supported automatically")
    message(STATUS "========================================")
else()
    message(WARNING ".NET SDK not found - Scripting will not be available")
endif()

# ======================= Game Project Symlink =========================
# Create symlinks for all game project folders so builds can access them
file(GLOB_RECURSE ALL_PROJECT_SETTINGS_FOR_COPY
    "${CMAKE_CURRENT_LIST_DIR}/*/ProjectSettings.json"
)

foreach(SETTINGS_FILE ${ALL_PROJECT_SETTINGS_FOR_COPY})
    get_filename_component(PROJECT_DIR ${SETTINGS_FILE} DIRECTORY)
    get_filename_component(PROJECT_NAME ${PROJECT_DIR} NAME)
    
    # Create symlink/junction for editor build (use a custom target that depends on the editor)
    if(BUILD_EDITOR AND TARGET GrapeEditor)
        string(REPLACE "/" "_" JUNCTION_SAFE_NAME "${PROJECT_NAME}")
        string(REPLACE "\\" "_" JUNCTION_SAFE_NAME "${JUNCTION_SAFE_NAME}")
        set(JUNCTION_TARGET_NAME "CreateJunction_${JUNCTION_SAFE_NAME}_Editor")

        if(WIN32)
            add_custom_target(${JUNCTION_TARGET_NAME} ALL
                COMMAND ${CMAKE_COMMAND} -E echo "Creating junction to ${PROJECT_NAME} (Editor)"
                COMMAND ${CMAKE_COMMAND} -E echo ""
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GrapeEditor>"
                COMMAND cmd /C "if not exist \"$<TARGET_FILE_DIR:GrapeEditor>\\${PROJECT_NAME}\" mklink /J \"$<TARGET_FILE_DIR:GrapeEditor>\\${PROJECT_NAME}\" \"${PROJECT_DIR}\""
                DEPENDS GrapeEditor
                COMMENT "Creating junction to ${PROJECT_NAME} project (Editor)"
            )
        else()
            add_custom_target(${JUNCTION_TARGET_NAME} ALL
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "${PROJECT_DIR}"
                    "$<TARGET_FILE_DIR:GrapeEditor>/${PROJECT_NAME}"
                DEPENDS GrapeEditor
                COMMENT "Creating symlink to ${PROJECT_NAME} project (Editor)"
            )
        endif()
    endif()

    # Create symlink/junction for game build (use a custom target that depends on the runtime)
    if(BUILD_GAME AND TARGET GrapeRuntime)
        string(REPLACE "/" "_" JUNCTION_SAFE_NAME_GAME "${PROJECT_NAME}")
        string(REPLACE "\\" "_" JUNCTION_SAFE_NAME_GAME "${JUNCTION_SAFE_NAME_GAME}")
        set(JUNCTION_TARGET_NAME_GAME "CreateJunction_${JUNCTION_SAFE_NAME_GAME}_Runtime")

        if(WIN32)
            add_custom_target(${JUNCTION_TARGET_NAME_GAME} ALL
                COMMAND ${CMAKE_COMMAND} -E echo "Creating junction to ${PROJECT_NAME} (Game)"
                COMMAND ${CMAKE_COMMAND} -E echo ""
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GrapeRuntime>"
                COMMAND cmd /C "if not exist \"$<TARGET_FILE_DIR:GrapeRuntime>\\${PROJECT_NAME}\" mklink /J \"$<TARGET_FILE_DIR:GrapeRuntime>\\${PROJECT_NAME}\" \"${PROJECT_DIR}\""
                DEPENDS GrapeRuntime
                COMMENT "Creating junction to ${PROJECT_NAME} project (Game)"
            )
        else()
            add_custom_target(${JUNCTION_TARGET_NAME_GAME} ALL
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "${PROJECT_DIR}"
                    "$<TARGET_FILE_DIR:GrapeRuntime>/${PROJECT_NAME}"
                DEPENDS GrapeRuntime
                COMMENT "Creating symlink to ${PROJECT_NAME} project (Game)"
            )
        endif()
    endif()
    
    message(STATUS "Symlink to '${PROJECT_NAME}' will be created in build directory")
endforeach()

if(BUILD_EDITOR AND BUILD_GAME)
    message(STATUS "GrapeEngine configured successfully! (Editor + Game)")
elseif(BUILD_EDITOR)
    message(STATUS "GrapeEngine configured successfully! (Editor only)")
elseif(BUILD_GAME)
    message(STATUS "GrapeEngine configured successfully! (Game only)")
endif()

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
