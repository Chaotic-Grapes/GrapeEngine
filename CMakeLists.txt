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

# ======================= BUILD CONFIGURATION =========================
# Option to build editor or standalone game
option(BUILD_EDITOR "Build the level editor (includes ImGui)" ON)
option(BUILD_GAME "Build the standalone game (no editor, no ImGui)" OFF)

if(NOT BUILD_EDITOR AND NOT BUILD_GAME)
    message(FATAL_ERROR "At least one of BUILD_EDITOR or BUILD_GAME must be enabled")
endif()

message(STATUS "Build Configuration:")
message(STATUS "  BUILD_EDITOR: ${BUILD_EDITOR}")
message(STATUS "  BUILD_GAME: ${BUILD_GAME}")

# ======================= COMMON SOURCE FILES =========================
# Collect engine source files (no editor, no game-specific entry points)
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/src/engine/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/engine/*.c
    ${CMAKE_CURRENT_LIST_DIR}/src/externals/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/externals/*.c
)

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/include/engine/*.h
    ${CMAKE_CURRENT_LIST_DIR}/include/engine/*.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/externals/*.h
    ${CMAKE_CURRENT_LIST_DIR}/include/externals/*.hpp
)

# Exclude editor-only external sources when building standalone game
set(ENGINE_SOURCES_FOR_LIB ${ENGINE_SOURCES})
if(BUILD_GAME AND NOT BUILD_EDITOR)
    # Exclude ImGuizmo (editor gizmo) which depends on ImGui
    list(FILTER ENGINE_SOURCES_FOR_LIB EXCLUDE REGEX ".*/ImGuizmo\\.cpp$")
    # Exclude DebugUI implementation from standalone game (editor-only)
    list(FILTER ENGINE_SOURCES_FOR_LIB EXCLUDE REGEX ".*/DebugUI\\.cpp$")
endif()

# ======================= ENGINE DLL =========================
message(STATUS "Configuring GrapeEngine as shared library (DLL)...")

# Create the engine as a shared library (DLL)
add_library(GrapeEngineLib SHARED ${ENGINE_SOURCES_FOR_LIB} ${ENGINE_HEADERS})

# Set C++ standard
set_property(TARGET GrapeEngineLib PROPERTY CXX_STANDARD 23)
set_property(TARGET GrapeEngineLib PROPERTY CXX_STANDARD_REQUIRED ON)

# Output name is always GrapeEngine.dll
set_target_properties(GrapeEngineLib PROPERTIES
    OUTPUT_NAME "GrapeEngine"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# Conditional ImGui support based on build configuration
if(BUILD_EDITOR)
    target_compile_definitions(GrapeEngineLib PUBLIC USE_IMGUI)
endif()

# Include directories
target_include_directories(GrapeEngineLib PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}/include/engine
    ${CMAKE_CURRENT_LIST_DIR}/include/externals
)

# Link libraries
if(BUILD_EDITOR)
    target_link_libraries(GrapeEngineLib
        PUBLIC
            glfw
            glad
            imgui
            glm
            freetype
            fmod
            ${NETHOST_LIB}
    )
else()
    target_link_libraries(GrapeEngineLib
        PUBLIC
            glfw
            glad
            glm
            freetype
            fmod
            ${NETHOST_LIB}
    )
endif()

message(STATUS "  Target: GrapeEngine.dll (Shared Library)")
if(BUILD_EDITOR)
    message(STATUS "  ImGui: Enabled")
else()
    message(STATUS "  ImGui: Disabled")
endif()

# ======================= EDITOR BUILD =========================
if(BUILD_EDITOR)
    message(STATUS "Configuring Editor Executable...")
    
    # Collect editor source files
    file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS 
        ${CMAKE_CURRENT_LIST_DIR}/src/editor/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/editor/*.c
    )
    
    file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS 
        ${CMAKE_CURRENT_LIST_DIR}/include/editor/*.h
        ${CMAKE_CURRENT_LIST_DIR}/include/editor/*.hpp
    )
    
    # Create editor executable (links to engine DLL)
    add_executable(GrapeEditor ${EDITOR_SOURCES} ${EDITOR_HEADERS})
    
    # Set C++ standard
    set_property(TARGET GrapeEditor PROPERTY CXX_STANDARD 23)
    set_property(TARGET GrapeEditor PROPERTY CXX_STANDARD_REQUIRED ON)
    
    # Include directories
    target_include_directories(GrapeEditor PRIVATE 
        ${CMAKE_CURRENT_LIST_DIR}/include/engine
        ${CMAKE_CURRENT_LIST_DIR}/include/editor
        ${CMAKE_CURRENT_LIST_DIR}/include/externals
    )
    
    # Link to engine DLL
    target_link_libraries(GrapeEditor PRIVATE GrapeEngineLib)
    
    message(STATUS "  Target: GrapeEditor.exe (Editor Executable)")
endif()

# ======================= STANDALONE GAME BUILD =========================
if(BUILD_GAME)
    message(STATUS "Configuring Standalone Game Build...")
    
    # Game entry point
    set(GAME_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/game/GameMain.cpp)
    
    # Create game executable (links to engine DLL, NO editor sources)
    add_executable(GrapeGame ${GAME_SOURCES})
    
    # Set C++ standard
    set_property(TARGET GrapeGame PROPERTY CXX_STANDARD 23)
    set_property(TARGET GrapeGame PROPERTY CXX_STANDARD_REQUIRED ON)

    # ------------------------------------------------------------------
    # Auto-detect or override output name for the game executable
    # If -DGAME_OUTPUT_NAME=<name> is provided, it will be used.
    # Otherwise attempt to find the first top-level project folder
    # (a folder containing `ProjectSettings.json`) and use its name.
    # Falls back to "GrapeGame" if nothing is found.
    # ------------------------------------------------------------------
    set(GAME_OUTPUT_NAME "" CACHE STRING "Name for the game executable (auto-detected from first game project if empty)")

    if(GAME_OUTPUT_NAME STREQUAL "")
        file(GLOB PROJECT_SETTINGS_FILES "${CMAKE_CURRENT_LIST_DIR}/*/ProjectSettings.json")
        # Prefer a project folder that contains a .csproj (explicit game project).
        # Failing that, prefer a folder whose name starts with a letter (avoid numeric/tooling folders).
        set(_preferred_with_csproj "")
        set(_preferred_alpha_name "")
        foreach(_settings_file ${PROJECT_SETTINGS_FILES})
            get_filename_component(_proj_dir ${_settings_file} DIRECTORY)
            get_filename_component(_proj_name ${_proj_dir} NAME)
            # Check for a .csproj in the project dir
            file(GLOB _csproj_files "${_proj_dir}/*.csproj")
            if(_csproj_files)
                set(_preferred_with_csproj "${_proj_name}")
                break()
            endif()
            # If no csproj yet, remember first name that starts with a letter
            if(_preferred_alpha_name STREQUAL "")
                string(REGEX MATCH "^[A-Za-z]" _starts_letter "${_proj_name}")
                if(_starts_letter)
                    set(_preferred_alpha_name "${_proj_name}")
                endif()
            endif()
        endforeach()

        if(NOT _preferred_with_csproj STREQUAL "")
            set(GAME_OUTPUT_NAME "${_preferred_with_csproj}")
        elseif(NOT _preferred_alpha_name STREQUAL "")
            set(GAME_OUTPUT_NAME "${_preferred_alpha_name}")
        else()
            # Fallback to first found project settings file's parent folder name
            if(PROJECT_SETTINGS_FILES)
                list(GET PROJECT_SETTINGS_FILES 0 FIRST_SETTINGS)
                get_filename_component(FIRST_PROJECT_DIR ${FIRST_SETTINGS} DIRECTORY)
                get_filename_component(FIRST_PROJECT_NAME ${FIRST_PROJECT_DIR} NAME)
                set(GAME_OUTPUT_NAME "${FIRST_PROJECT_NAME}")
            else()
                set(GAME_OUTPUT_NAME "GrapeGame")
            endif()
        endif()

        message(STATUS "Auto-detected game project name: ${GAME_OUTPUT_NAME}")
    endif()

    # Append suffix "Game" to the base project name unless it already ends
    # with "game" (case-insensitive). This makes the standalone executable
    # name explicit (e.g. EchoesBelow -> EchoesBelowGame).
    set(_GAME_NAME_BASE "${GAME_OUTPUT_NAME}")
    string(TOLOWER _GAME_NAME_LOWER "${_GAME_NAME_BASE}")
    string(LENGTH _game_name_len "${_GAME_NAME_LOWER}")
    if(_game_name_len GREATER_EQUAL 4)
        math(EXPR _start_idx "${_game_name_len} - 4")
        string(SUBSTRING _GAME_NAME_LOWER ${_start_idx} 4 _GAME_NAME_TAIL)
    else()
        set(_GAME_NAME_TAIL "")
    endif()

    if(NOT _GAME_NAME_TAIL STREQUAL "game")
        set(GAME_FINAL_OUTPUT_NAME "${_GAME_NAME_BASE}Game")
    else()
        set(GAME_FINAL_OUTPUT_NAME "${_GAME_NAME_BASE}")
    endif()

    # Apply the chosen name to the built executable and MSVC PDBs
    set_target_properties(GrapeGame PROPERTIES
        OUTPUT_NAME "${GAME_FINAL_OUTPUT_NAME}"
        PDB_NAME "${GAME_FINAL_OUTPUT_NAME}"
        COMPILE_PDB_NAME "${GAME_FINAL_OUTPUT_NAME}"
        COMPILE_PDB_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:GrapeGame>"
    )

    # MSVC / Visual Studio generator sometimes produces project-level TargetName
    # values with numeric prefixes which end up in the .exe filename (e.g.
    # "14EchoesBelowGame.exe") even when OUTPUT_NAME is set. To normalize the
    # on-disk executable name across generators, add a post-build rename step
    # that forces the final exe to match ${GAME_FINAL_OUTPUT_NAME}. This is a
    # harmless safety measure and only affects the standalone game target.
    if(MSVC)
        add_custom_command(TARGET GrapeGame POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Normalizing game executable name to ${GAME_FINAL_OUTPUT_NAME}.exe"
            COMMAND ${CMAKE_COMMAND} -E rename "$<TARGET_FILE:GrapeGame>" "$<TARGET_FILE_DIR:GrapeGame>/${GAME_FINAL_OUTPUT_NAME}.exe"
            COMMENT "Normalize game executable name"
        )
    endif()
    
    # NO USE_IMGUI definition - this disables all ImGui code
    # target_compile_definitions(GrapeGame PRIVATE ...) - intentionally empty
    
    # Include directories (NO editor includes)
    target_include_directories(GrapeGame PRIVATE 
        ${CMAKE_CURRENT_LIST_DIR}/include/engine
        ${CMAKE_CURRENT_LIST_DIR}/include/externals
    )
    
    # Link to engine DLL
    target_link_libraries(GrapeGame PRIVATE GrapeEngineLib)
    
    message(STATUS "  Target: ${GAME_FINAL_OUTPUT_NAME} (Standalone)")
    message(STATUS "  Linked to: GrapeEngine.dll")
endif()

# Set the primary target based on what's being built
if(BUILD_EDITOR)
    set(PRIMARY_TARGET GrapeEditor)
    set(PRIMARY_LIB GrapeEngineLib)
else()
    set(PRIMARY_TARGET GrapeGame)
    set(PRIMARY_LIB GrapeEngineLib)
endif()

# ======================= SCRIPT API EXPORTS =========================
# Export functions from the engine DLL for C# P/Invoke
# With WINDOWS_EXPORT_ALL_SYMBOLS ON, all SCRIPT_API functions are automatically exported.
# No manual /EXPORT flags needed - the DLL exports symbols naturally.
message(STATUS "Script API will be exported from GrapeEngine.dll")

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
    
    # Compiler warnings for editor
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(GrapeEditor PRIVATE -Wall -Wextra -Wpedantic -Werror)
    elseif (MSVC)
        target_compile_options(GrapeEditor PRIVATE /W4)
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
    
    # Compiler warnings for game
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(GrapeGame PRIVATE -Wall -Wextra -Wpedantic -Werror)
    elseif (MSVC)
        target_compile_options(GrapeGame PRIVATE /W4)
    endif()
endif()

# ======================= Copy files and assets =========================

# Copy assets to build directory for editor
if(BUILD_EDITOR AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
    add_custom_command(TARGET GrapeEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:GrapeEditor>/assets"
        COMMENT "Copying assets to editor build directory"
    )
    add_custom_command(TARGET GrapeEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "${CMAKE_BINARY_DIR}/assets"
        COMMENT "Copying assets to project folder"
    )
endif()

# Copy assets to build directory for game
if(BUILD_GAME AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
    add_custom_command(TARGET GrapeGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:GrapeGame>/assets"
        COMMENT "Copying assets to game build directory"
    )
endif()

# Copy FMOD DLL (Windows) for editor
if(BUILD_EDITOR)
    add_custom_command(TARGET GrapeEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/lib/fmod/fmod.dll"
            "$<TARGET_FILE_DIR:GrapeEditor>"
        COMMENT "Copying FMOD DLL to editor output directory"
    )
endif()

# Copy FMOD DLL (Windows) for game
if(BUILD_GAME)
    add_custom_command(TARGET GrapeGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/lib/fmod/fmod.dll"
            "$<TARGET_FILE_DIR:GrapeGame>"
        COMMENT "Copying FMOD DLL to game output directory"
    )
endif()

# Copy nethost.dll for CoreCLR hosting (Windows) for editor
if(BUILD_EDITOR AND EXISTS "${CORECLR_LIB_DIR}/nethost.dll")
    add_custom_command(TARGET GrapeEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CORECLR_LIB_DIR}/nethost.dll"
            "$<TARGET_FILE_DIR:GrapeEditor>"
        COMMENT "Copying nethost.dll to editor output directory"
    )
endif()

# Copy nethost.dll for CoreCLR hosting (Windows) for game
if(BUILD_GAME AND EXISTS "${CORECLR_LIB_DIR}/nethost.dll")
    add_custom_command(TARGET GrapeGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CORECLR_LIB_DIR}/nethost.dll"
            "$<TARGET_FILE_DIR:GrapeGame>"
        COMMENT "Copying nethost.dll to game output directory"
    )
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
    
    # Build GrapeEngine.ScriptAPI (engine's API)
    add_custom_target(
        BuildScriptAPI ALL
        COMMAND ${DOTNET_EXECUTABLE} restore "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI/GrapeEngine.ScriptAPI.csproj"
        COMMAND ${DOTNET_EXECUTABLE} build "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI/GrapeEngine.ScriptAPI.csproj" 
            --configuration $<IF:$<CONFIG:Debug>,Debug,Release>
            --output "${CMAKE_BINARY_DIR}/$<CONFIG>"
            --no-restore
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/scripts"
        COMMENT "Building GrapeEngine Scripting API..."
        VERBATIM
    )
    
    # TODO: Remove when editor is separated - auto-discover game project folders
    # Find all ProjectSettings.json files (indicates a game project folder)
    file(GLOB_RECURSE PROJECT_SETTINGS_FILES 
        "${CMAKE_CURRENT_LIST_DIR}/*/ProjectSettings.json"
    )
    
    set(GAME_SCRIPT_TARGETS "")
    foreach(SETTINGS_FILE ${PROJECT_SETTINGS_FILES})
        get_filename_component(PROJECT_DIR ${SETTINGS_FILE} DIRECTORY)
        get_filename_component(PROJECT_NAME ${PROJECT_DIR} NAME)
        
        # Find ALL .cs files anywhere within the project folder
        # Developers can organize scripts however they want - no enforced structure
        file(GLOB_RECURSE CS_FILES "${PROJECT_DIR}/*.cs")
        
        # Exclude .cs files in build directories
        set(FILTERED_CS_FILES "")
        foreach(CS_FILE ${CS_FILES})
            string(FIND "${CS_FILE}" "/build/" IN_BUILD)
            string(FIND "${CS_FILE}" "\\build\\" IN_BUILD_WIN)
            if(IN_BUILD EQUAL -1 AND IN_BUILD_WIN EQUAL -1)
                list(APPEND FILTERED_CS_FILES ${CS_FILE})
            endif()
        endforeach()
        
        if(FILTERED_CS_FILES)
            list(LENGTH FILTERED_CS_FILES CS_COUNT)
            
            # Look for an existing .csproj file in the project directory
            set(PROJECT_CSPROJ "${PROJECT_DIR}/${PROJECT_NAME}.csproj")
            
            if(EXISTS "${PROJECT_CSPROJ}")
                message(STATUS "  Found game project: ${PROJECT_NAME} with ${CS_COUNT} script(s) (using ${PROJECT_NAME}.csproj)")
                
                # Use the existing .csproj file
                add_custom_target(
                    Build_${PROJECT_NAME}Scripts ALL
                    COMMAND ${DOTNET_EXECUTABLE} build "${PROJECT_CSPROJ}"
                        --configuration $<IF:$<CONFIG:Debug>,Debug,Release>
                        --output "${CMAKE_BINARY_DIR}/$<CONFIG>"
                    WORKING_DIRECTORY "${PROJECT_DIR}"
                    COMMENT "Building ${PROJECT_NAME} scripts (${CS_COUNT} files)..."
                    DEPENDS BuildScriptAPI
                    VERBATIM
                )
            else()
                message(STATUS "  Found game project: ${PROJECT_NAME} with ${CS_COUNT} script(s) (no .csproj - auto-discovery)")
                message(STATUS "    Tip: Create ${PROJECT_NAME}.csproj for IDE support and hot reload")
                
                # Build directly - output to build/Debug or build/Release based on config
                if(CMAKE_CONFIGURATION_TYPES)
                    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/Debug")  # Default to Debug for multi-config
                else()
                    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
                endif()
                set(SCRIPTAPI_DLL_PATH "${OUTPUT_DIR}/GrapeEngine.ScriptAPI.dll")
                
                # Generate a temporary .csproj for this game project
                set(TEMP_CSPROJ "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_Generated.csproj")
                file(WRITE ${TEMP_CSPROJ} "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
                file(APPEND ${TEMP_CSPROJ} "  <PropertyGroup>\n")
                file(APPEND ${TEMP_CSPROJ} "    <TargetFramework>${DOTNET_TFM}</TargetFramework>\n")
                file(APPEND ${TEMP_CSPROJ} "    <ImplicitUsings>enable</ImplicitUsings>\n")
                file(APPEND ${TEMP_CSPROJ} "    <Nullable>enable</Nullable>\n")
                file(APPEND ${TEMP_CSPROJ} "    <AssemblyName>${PROJECT_NAME}</AssemblyName>\n")
                file(APPEND ${TEMP_CSPROJ} "    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n")
                file(APPEND ${TEMP_CSPROJ} "  </PropertyGroup>\n")
                file(APPEND ${TEMP_CSPROJ} "  <ItemGroup>\n")
                foreach(CS_FILE ${FILTERED_CS_FILES})
                    file(APPEND ${TEMP_CSPROJ} "    <Compile Include=\"${CS_FILE}\" />\n")
                endforeach()
                file(APPEND ${TEMP_CSPROJ} "  </ItemGroup>\n")
                file(APPEND ${TEMP_CSPROJ} "  <ItemGroup>\n")
                file(APPEND ${TEMP_CSPROJ} "    <Reference Include=\"GrapeEngine.ScriptAPI\">\n")
                file(APPEND ${TEMP_CSPROJ} "      <HintPath>${SCRIPTAPI_DLL_PATH}</HintPath>\n")
                file(APPEND ${TEMP_CSPROJ} "    </Reference>\n")
                file(APPEND ${TEMP_CSPROJ} "  </ItemGroup>\n")
                file(APPEND ${TEMP_CSPROJ} "</Project>\n")
                
                add_custom_target(
                    Build_${PROJECT_NAME}Scripts ALL
                    COMMAND ${DOTNET_EXECUTABLE} build "${TEMP_CSPROJ}"
                        --configuration Debug
                        --output "${OUTPUT_DIR}"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                    COMMENT "Building ${PROJECT_NAME} scripts (${CS_COUNT} files)..."
                    DEPENDS BuildScriptAPI ${FILTERED_CS_FILES}
                    VERBATIM
                )
            endif()
            
            list(APPEND GAME_SCRIPT_TARGETS Build_${PROJECT_NAME}Scripts)
        endif()
    endforeach()
    
    # Also find standalone .csproj files (for developers who prefer manual control)
    file(GLOB_RECURSE ALL_CSPROJ_FILES 
        "${CMAKE_CURRENT_LIST_DIR}/*.csproj"
    )
    
    foreach(CSPROJ_FILE ${ALL_CSPROJ_FILES})
        get_filename_component(PROJ_NAME ${CSPROJ_FILE} NAME_WE)
        get_filename_component(PROJ_DIR ${CSPROJ_FILE} DIRECTORY)
        
        # Skip engine API, build dirs, and auto-generated project files
        string(FIND "${PROJ_DIR}" "/build/" IN_BUILD_DIR)
        string(FIND "${PROJ_DIR}" "\\build\\" IN_BUILD_DIR_WIN)
        string(FIND "${PROJ_NAME}" "Scripts" HAS_SCRIPTS_SUFFIX)
        
        if(NOT PROJ_NAME STREQUAL "GrapeEngine.ScriptAPI" AND 
           IN_BUILD_DIR EQUAL -1 AND IN_BUILD_DIR_WIN EQUAL -1 AND
           HAS_SCRIPTS_SUFFIX EQUAL -1)
            
            file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_LIST_DIR}" "${CSPROJ_FILE}")
            message(STATUS "  Found standalone script project: ${PROJ_NAME} (${REL_PATH})")
            
            add_custom_target(
                Build_${PROJ_NAME} ALL
                COMMAND ${DOTNET_EXECUTABLE} build "${CSPROJ_FILE}"
                    --configuration $<IF:$<CONFIG:Debug>,Debug,Release>
                    --output "${CMAKE_BINARY_DIR}/$<CONFIG>"
                WORKING_DIRECTORY "${PROJ_DIR}"
                COMMENT "Building ${PROJ_NAME}..."
                DEPENDS BuildScriptAPI
                VERBATIM
            )
            
            list(APPEND GAME_SCRIPT_TARGETS Build_${PROJ_NAME})
        endif()
    endforeach()
    
    # Make targets depend on all script builds
    if(BUILD_EDITOR)
        add_dependencies(GrapeEditor BuildScriptAPI ${GAME_SCRIPT_TARGETS})
    endif()
    if(BUILD_GAME)
        add_dependencies(GrapeGame BuildScriptAPI ${GAME_SCRIPT_TARGETS})
    endif()
    
    # Note: C# DLLs are already built directly to ${CMAKE_BINARY_DIR}/$<CONFIG>
    # which is the same directory as the executable, so no copying is needed.

    # Copy runtimeconfig.json for .NET hosting (editor)
    if(BUILD_EDITOR AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI.runtimeconfig.json")
        add_custom_command(TARGET GrapeEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI.runtimeconfig.json"
                "$<TARGET_FILE_DIR:GrapeEditor>"
            COMMENT "Copying runtimeconfig.json to editor output directory"
        )
    endif()
    
    # Copy runtimeconfig.json for .NET hosting (game)
    if(BUILD_GAME AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI.runtimeconfig.json")
        add_custom_command(TARGET GrapeGame POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_LIST_DIR}/scripts/GrapeEngine.ScriptAPI.runtimeconfig.json"
                "$<TARGET_FILE_DIR:GrapeGame>"
            COMMENT "Copying runtimeconfig.json to game output directory"
        )
    endif()
    
    message(STATUS "C# build output: ${CMAKE_BINARY_DIR}/$<CONFIG>")
    message(STATUS "")
    message(STATUS "Script Development:")
    message(STATUS "  - Create a game project folder with ProjectSettings.json")
    message(STATUS "  - Add .cs files ANYWHERE in the project folder")
    message(STATUS "  - Scripts compiled directly")
    message(STATUS "  - Example: EchoesBelow/Player.cs, EchoesBelow/Code/Enemy.cs, etc.")
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
    
    # Create symlink/junction for editor build
    if(BUILD_EDITOR)
        if(WIN32)
            # Use directory junction on Windows (mklink /J) to avoid requiring developer-mode symlink privileges
            add_custom_command(TARGET GrapeEditor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Creating junction to ${PROJECT_NAME} (Editor)"
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GrapeEditor>"
                COMMAND cmd /C "if not exist \"$<TARGET_FILE_DIR:GrapeEditor>\\${PROJECT_NAME}\" mklink /J \"$<TARGET_FILE_DIR:GrapeEditor>\\${PROJECT_NAME}\" \"${PROJECT_DIR}\""
                COMMENT "Creating junction to ${PROJECT_NAME} project (Editor)"
            )
        else()
            add_custom_command(TARGET GrapeEditor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                "${PROJECT_DIR}"
                "$<TARGET_FILE_DIR:GrapeEditor>/${PROJECT_NAME}"
                COMMENT "Creating symlink to ${PROJECT_NAME} project (Editor)"
            )
        endif()
    endif()

    # Create symlink/junction for game build
    if(BUILD_GAME)
        if(WIN32)
            add_custom_command(TARGET GrapeGame POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Creating junction to ${PROJECT_NAME} (Game)"
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GrapeGame>"
                COMMAND cmd /C "if not exist \"$<TARGET_FILE_DIR:GrapeGame>\\${PROJECT_NAME}\" mklink /J \"$<TARGET_FILE_DIR:GrapeGame>\\${PROJECT_NAME}\" \"${PROJECT_DIR}\""
                COMMENT "Creating junction to ${PROJECT_NAME} project (Game)"
            )
        else()
            add_custom_command(TARGET GrapeGame POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                "${PROJECT_DIR}"
                "$<TARGET_FILE_DIR:GrapeGame>/${PROJECT_NAME}"
                COMMENT "Creating symlink to ${PROJECT_NAME} project (Game)"
            )
        endif()
    endif()
    
    message(STATUS "Symlink to '${PROJECT_NAME}' will be created in build directory")
endforeach()

message(STATUS "")
message(STATUS "========================================")
message(STATUS "GrapeEngine Build Configuration:")
message(STATUS "  Engine: GrapeEngine.dll (Shared Library)")
if(BUILD_EDITOR)
    message(STATUS "  Editor: GrapeEditor.exe")
endif()
if(BUILD_GAME)
    message(STATUS "  Game: ${GAME_FINAL_OUTPUT_NAME}.exe")
endif()
message(STATUS "========================================")
message(STATUS "")

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