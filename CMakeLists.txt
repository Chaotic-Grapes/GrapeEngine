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
        ${NETHOST_LIB}
)

# Export functions from executable for C# P/Invoke
# Note: This is temporary until we refactor to a DLL!
if(MSVC)
    set_target_properties(GrapeEngine PROPERTIES
        LINK_FLAGS "/EXPORT:ScriptAPI_GetComponent /EXPORT:ScriptAPI_GetComponentPtr /EXPORT:ScriptAPI_AddComponent /EXPORT:ScriptAPI_SetComponent /EXPORT:ScriptAPI_HasComponent /EXPORT:ScriptAPI_RemoveComponent /EXPORT:ScriptAPI_CreateEntity /EXPORT:ScriptAPI_DestroyEntity /EXPORT:ScriptAPI_IsAlive /EXPORT:ScriptAPI_SetWorld /EXPORT:ScriptAPI_IsKeyPressed /EXPORT:ScriptAPI_IsKeyDown /EXPORT:ScriptAPI_IsKeyUp /EXPORT:ScriptAPI_IsMousePressed /EXPORT:ScriptAPI_GetMouseX /EXPORT:ScriptAPI_GetMouseY /EXPORT:ScriptAPI_GetScrollX /EXPORT:ScriptAPI_GetScrollY /EXPORT:ScriptAPI_GetWindowWidth /EXPORT:ScriptAPI_GetWindowHeight /EXPORT:ScriptAPI_Time_GetDeltaTime /EXPORT:ScriptAPI_Time_GetUnscaledDeltaTime /EXPORT:ScriptAPI_Time_GetFixedDeltaTime /EXPORT:ScriptAPI_Time_GetUnscaledFixedDeltaTime /EXPORT:ScriptAPI_Time_GetElapsedTime /EXPORT:ScriptAPI_Time_GetFrameCount /EXPORT:ScriptAPI_Time_GetTimeScale /EXPORT:ScriptAPI_Time_SetTimeScale /EXPORT:ScriptAPI_Time_GetMaximumDeltaTime /EXPORT:ScriptAPI_Time_SetMaximumDeltaTime"
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
# Build the engine's scripting API AND discover ALL .cs files in game projects
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
            message(STATUS "  Found game project: ${PROJECT_NAME} with ${CS_COUNT} script(s)")
            
            # Build directly using csc.exe without needing .csproj files
            # Reference the ScriptAPI DLL that was built above
            set(OUTPUT_DLL "${CMAKE_BINARY_DIR}/$<CONFIG>/${PROJECT_NAME}.dll")
            set(SCRIPTAPI_DLL "${CMAKE_BINARY_DIR}/$<CONFIG>/GrapeEngine.ScriptAPI.dll")
            
            # Convert CS files list to space-separated string
            string(REPLACE ";" " " CS_FILES_STR "${FILTERED_CS_FILES}")
            
            # Create a build target that compiles .cs files directly
            add_custom_target(
                Build_${PROJECT_NAME}Scripts ALL
                COMMAND ${DOTNET_EXECUTABLE} exec 
                    $ENV{ProgramFiles}/dotnet/sdk/9.0.*/Roslyn/bincore/csc.dll
                    /target:library
                    /out:${OUTPUT_DLL}
                    /reference:${SCRIPTAPI_DLL}
                    /langversion:latest
                    /nullable:enable
                    ${FILTERED_CS_FILES}
                WORKING_DIRECTORY "${PROJECT_DIR}"
                COMMENT "Compiling ${PROJECT_NAME} scripts (${CS_COUNT} files)..."
                DEPENDS BuildScriptAPI
                VERBATIM
            )
            
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
# TODO: Remove this section when editor is separated from engine
# Create symlinks for all game project folders so editor can access them from build directory
file(GLOB_RECURSE ALL_PROJECT_SETTINGS_FOR_COPY
    "${CMAKE_CURRENT_LIST_DIR}/*/ProjectSettings.json"
)

foreach(SETTINGS_FILE ${ALL_PROJECT_SETTINGS_FOR_COPY})
    get_filename_component(PROJECT_DIR ${SETTINGS_FILE} DIRECTORY)
    get_filename_component(PROJECT_NAME ${PROJECT_DIR} NAME)
    
    # Create symlink instead of copying for faster builds and real-time file access
    add_custom_command(TARGET GrapeEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${PROJECT_DIR}"
        "$<TARGET_FILE_DIR:GrapeEngine>/${PROJECT_NAME}"
        COMMENT "Creating symlink to ${PROJECT_NAME} project"
    )
    message(STATUS "Symlink to '${PROJECT_NAME}' will be created in build directory")
endforeach()

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