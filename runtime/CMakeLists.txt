# ======================= Runtime/Game Executable Configuration =========================
# Runtime is a standalone C++ game player that links against GrapeEngineLib.
# It has NO editor dependencies and NO ImGui - pure game execution.
#
# This is what ships to end users. It loads:
# - Pre-compiled C# script assemblies (GameScripts.dll)
# - Processed game assets
# - Game configuration
# ========================================================================================

cmake_minimum_required(VERSION 3.13)

project(GrapeRuntime VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ======================= Game Entry Point =========================
set(RUNTIME_SOURCES 
    ${CMAKE_CURRENT_LIST_DIR}/../src/game/GameMain.cpp
    # Force global new/delete override to be compiled into the Runtime EXE
    ${CMAKE_CURRENT_LIST_DIR}/../engine/src/services/GlobalMemoryOverride.cpp
)

# ======================= Create Runtime Executable =========================
add_executable(GrapeRuntime ${RUNTIME_SOURCES})

# ======================= Auto-detect Game Project Name =========================
# Try to find a game project folder containing ProjectSettings.json
set(GAME_OUTPUT_NAME "" CACHE STRING "Name for the game executable (auto-detected if empty)")

if(GAME_OUTPUT_NAME STREQUAL "")
    file(GLOB PROJECT_SETTINGS_FILES "${CMAKE_CURRENT_LIST_DIR}/../*/ProjectSettings.json")
    
    # Prefer a project folder that contains a .csproj (explicit game project)
    set(_preferred_with_csproj "")
    set(_preferred_alpha_name "")
    
    foreach(_settings_file ${PROJECT_SETTINGS_FILES})
        get_filename_component(_proj_dir ${_settings_file} DIRECTORY)
        get_filename_component(_proj_name ${_proj_dir} NAME)
        
        # Check for .csproj in the project dir
        file(GLOB _csproj_files "${_proj_dir}/*.csproj")
        if(_csproj_files)
            set(_preferred_with_csproj "${_proj_name}")
            break()
        endif()
        
        # Remember first name that starts with a letter
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
        # Fallback to first found or default
        if(PROJECT_SETTINGS_FILES)
            list(GET PROJECT_SETTINGS_FILES 0 FIRST_SETTINGS)
            get_filename_component(FIRST_PROJECT_DIR ${FIRST_SETTINGS} DIRECTORY)
            get_filename_component(FIRST_PROJECT_NAME ${FIRST_PROJECT_DIR} NAME)
            set(GAME_OUTPUT_NAME "${FIRST_PROJECT_NAME}")
        else()
            set(GAME_OUTPUT_NAME "GrapeGame")
        endif()
    endif()
    
    message(STATUS "Runtime: Auto-detected game project: ${GAME_OUTPUT_NAME}")
endif()

# Append "Game" suffix if not already present
set(_GAME_NAME_BASE "${GAME_OUTPUT_NAME}")
string(TOLOWER "${_GAME_NAME_BASE}" _GAME_NAME_LOWER)
string(LENGTH "${_GAME_NAME_LOWER}" _game_name_len)

if(_game_name_len GREATER_EQUAL 4)
    math(EXPR _start_idx "${_game_name_len} - 4")
    string(SUBSTRING "${_GAME_NAME_LOWER}" ${_start_idx} 4 _GAME_NAME_TAIL)
else()
    set(_GAME_NAME_TAIL "")
endif()

if(NOT _GAME_NAME_TAIL STREQUAL "game")
    set(GAME_FINAL_OUTPUT_NAME "${_GAME_NAME_BASE}Game")
else()
    set(GAME_FINAL_OUTPUT_NAME "${_GAME_NAME_BASE}")
endif()

# Apply the chosen name
set_target_properties(GrapeRuntime PROPERTIES
    OUTPUT_NAME "${GAME_FINAL_OUTPUT_NAME}"
    PDB_NAME "${GAME_FINAL_OUTPUT_NAME}"
    COMPILE_PDB_NAME "${GAME_FINAL_OUTPUT_NAME}"
    COMPILE_PDB_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:GrapeRuntime>"
)

# Make Release builds a GUI/Windows app (no console for end users)
# Keep Debug builds as console apps for developer logging
set_target_properties(GrapeRuntime PROPERTIES 
    WIN32_EXECUTABLE "$<$<CONFIG:Release>:TRUE>"
)

# Set Visual Studio debugger working directory to build output
# This ensures assets and config files are found when debugging via .sln
set_target_properties(GrapeRuntime PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:GrapeRuntime>"
)

# ======================= Embed Icon (Windows) =========================
set(GAME_ICON "${CMAKE_CURRENT_LIST_DIR}/../assets/game_icon.ico" CACHE FILEPATH "Icon file for the game executable")

if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/../app_game.rc.in" 
        "${CMAKE_BINARY_DIR}/app_game.rc" 
        @ONLY
    )
    target_sources(GrapeRuntime PRIVATE "${CMAKE_BINARY_DIR}/app_game.rc")
    
    if(NOT EXISTS "${GAME_ICON}")
        message(WARNING "Game icon not found: ${GAME_ICON}")
    endif()
endif()

# ======================= Normalize Executable Name (MSVC) =========================
# MSVC sometimes adds numeric prefixes - force consistent naming
if(MSVC)
    add_custom_command(TARGET GrapeRuntime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Normalizing game executable name to ${GAME_FINAL_OUTPUT_NAME}.exe"
        COMMAND ${CMAKE_COMMAND} -E rename 
            "$<TARGET_FILE:GrapeRuntime>" 
            "$<TARGET_FILE_DIR:GrapeRuntime>/${GAME_FINAL_OUTPUT_NAME}.exe"
        COMMENT "Normalize game executable name"
    )
    
    # Provide GrapeEngine.exe alias for P/Invoke compatibility
    add_custom_command(TARGET GrapeRuntime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Providing GrapeEngine.exe alias for scripting interop"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            "$<TARGET_FILE_DIR:GrapeRuntime>/${GAME_FINAL_OUTPUT_NAME}.exe" 
            "$<TARGET_FILE_DIR:GrapeRuntime>/GrapeEngine.exe"
        COMMENT "Create GrapeEngine.exe alias"
    )
endif()

# ======================= Compiler Definitions =========================
# NO USE_IMGUI - runtime has no editor UI

# ======================= IDE Organization - Visual Studio Filters =========================
# Organize runtime source files by folder structure
foreach(source IN LISTS RUNTIME_SOURCES)
    get_filename_component(source_path "${source}" PATH)
    file(RELATIVE_PATH source_path_rel "${CMAKE_CURRENT_LIST_DIR}/../src" "${source_path}")
    if(NOT source_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Runtime Source\\${source_path_rel}")
    else()
        set(group_path "Runtime Source")
    endif()
    source_group("${group_path}" FILES "${source}")
endforeach()

# ======================= Link Dependencies =========================
target_link_libraries(GrapeRuntime
    PRIVATE
        GrapeEngineLib
)

# ======================= Script API Exports =========================
# Export C# P/Invoke functions from runtime executable
if(MSVC AND NOT BUILD_ENGINE_SHARED)
    set(SCRIPT_EXPORTS "/EXPORT:ScriptAPI_GetComponent /EXPORT:ScriptAPI_GetComponentPtr /EXPORT:ScriptAPI_AddComponent /EXPORT:ScriptAPI_SetComponent /EXPORT:ScriptAPI_HasComponent /EXPORT:ScriptAPI_RemoveComponent /EXPORT:ScriptAPI_CreateEntity /EXPORT:ScriptAPI_DestroyEntity /EXPORT:ScriptAPI_IsAlive /EXPORT:ScriptAPI_SetWorld /EXPORT:ScriptAPI_IsKeyPressed /EXPORT:ScriptAPI_IsKeyDown /EXPORT:ScriptAPI_IsKeyUp /EXPORT:ScriptAPI_IsMousePressed /EXPORT:ScriptAPI_GetMouseX /EXPORT:ScriptAPI_GetMouseY /EXPORT:ScriptAPI_GetScrollX /EXPORT:ScriptAPI_GetScrollY /EXPORT:ScriptAPI_Time_GetDeltaTime /EXPORT:ScriptAPI_Time_GetUnscaledDeltaTime /EXPORT:ScriptAPI_Time_GetFixedDeltaTime /EXPORT:ScriptAPI_Time_GetUnscaledFixedDeltaTime /EXPORT:ScriptAPI_Time_GetElapsedTime /EXPORT:ScriptAPI_Time_GetFrameCount /EXPORT:ScriptAPI_Time_GetTimeScale /EXPORT:ScriptAPI_Time_SetTimeScale /EXPORT:ScriptAPI_Time_GetMaximumDeltaTime /EXPORT:ScriptAPI_Time_SetMaximumDeltaTime /EXPORT:ScriptAPI_Audio_LoadCue /EXPORT:ScriptAPI_Audio_UnloadCue /EXPORT:ScriptAPI_Audio_HasCue /EXPORT:ScriptAPI_Audio_Play /EXPORT:ScriptAPI_Audio_PlaySingle /EXPORT:ScriptAPI_Audio_Stop /EXPORT:ScriptAPI_Audio_StopCue /EXPORT:ScriptAPI_Audio_IsCuePlaying /EXPORT:ScriptAPI_Audio_SetInstanceVolume /EXPORT:ScriptAPI_Audio_SetInstancePitch /EXPORT:ScriptAPI_Audio_SetInstancePosition /EXPORT:ScriptAPI_Audio_SetMasterVolume /EXPORT:ScriptAPI_Audio_GetMasterVolume /EXPORT:ScriptAPI_Audio_SetListener /EXPORT:ScriptAPI_Physics_SetGravity /EXPORT:ScriptAPI_Physics_GetGravity /EXPORT:ScriptAPI_Physics_SetEnabled /EXPORT:ScriptAPI_Physics_IsEnabled /EXPORT:ScriptAPI_Physics_ApplyForce /EXPORT:ScriptAPI_Physics_ApplyImpulse /EXPORT:ScriptAPI_Physics_GetVelocity /EXPORT:ScriptAPI_Physics_SetVelocity /EXPORT:ScriptAPI_UIEvents_Clear /EXPORT:ScriptAPI_UIEvents_WasClicked /EXPORT:ScriptAPI_UIEvents_WasHoverEntered /EXPORT:ScriptAPI_UIEvents_WasHoverExited /EXPORT:ScriptAPI_UIEvents_GetEventCount /EXPORT:ScriptAPI_UIEvents_GetEvent /EXPORT:ScriptAPI_UIEvents_GetHoveredEntity /EXPORT:ScriptAPI_UIEvents_IsMouseOverUI /EXPORT:ScriptAPI_Math_RandomInt /EXPORT:ScriptAPI_Math_RandomFloat /EXPORT:ScriptAPI_Math_RandomIntSeeded /EXPORT:ScriptAPI_Math_RandomFloatSeeded /EXPORT:ScriptAPI_Math_Clamp /EXPORT:ScriptAPI_Math_ClampInt /EXPORT:ScriptAPI_Math_Lerp /EXPORT:ScriptAPI_Math_Abs /EXPORT:ScriptAPI_Math_Sqrt /EXPORT:ScriptAPI_Math_Pow /EXPORT:ScriptAPI_Math_Round /EXPORT:ScriptAPI_Math_Floor /EXPORT:ScriptAPI_Math_Ceil /EXPORT:ScriptAPI_Math_Min /EXPORT:ScriptAPI_Math_Max /EXPORT:ScriptAPI_Math_Sin /EXPORT:ScriptAPI_Math_Cos /EXPORT:ScriptAPI_Math_Tan /EXPORT:ScriptAPI_Math_Asin /EXPORT:ScriptAPI_Math_Acos /EXPORT:ScriptAPI_Math_Atan /EXPORT:ScriptAPI_Math_Atan2 /EXPORT:ScriptAPI_Math_DegToRad /EXPORT:ScriptAPI_Math_RadToDeg /EXPORT:ScriptAPI_Math_Distance2D /EXPORT:ScriptAPI_Math_DistanceSquared2D /EXPORT:ScriptAPI_Math_Length2D /EXPORT:ScriptAPI_Math_LengthSquared2D /EXPORT:ScriptAPI_Math_Normalize2D /EXPORT:ScriptAPI_Math_Dot2D /EXPORT:ScriptAPI_Math_Cross2D /EXPORT:ScriptAPI_Math_Lerp2D /EXPORT:ScriptAPI_Math_ClampVector2D /EXPORT:ScriptAPI_Math_AngleTo /EXPORT:ScriptAPI_Math_DirectionFromAngle /EXPORT:ScriptAPI_Math_Rotate2D /EXPORT:ScriptAPI_Debug_LogInfo /EXPORT:ScriptAPI_Debug_LogDebug /EXPORT:ScriptAPI_Debug_LogWarning /EXPORT:ScriptAPI_Debug_LogError /EXPORT:ScriptAPI_Window_GetWidth /EXPORT:ScriptAPI_Window_GetHeight /EXPORT:ScriptAPI_Window_ShouldClose /EXPORT:ScriptAPI_Window_Close /EXPORT:ScriptAPI_Window_IsFocused /EXPORT:ScriptAPI_Window_IsMinimized /EXPORT:ScriptAPI_Window_SetMinimized /EXPORT:ScriptAPI_Window_IsMaximized /EXPORT:ScriptAPI_Window_SetMaximized /EXPORT:ScriptAPI_Window_IsVisible /EXPORT:ScriptAPI_Window_SetVisible /EXPORT:ScriptAPI_Window_IsResizable /EXPORT:ScriptAPI_Window_SetResizable /EXPORT:ScriptAPI_Window_SetMode /EXPORT:ScriptAPI_Window_HasMode /EXPORT:ScriptAPI_Window_Resize /EXPORT:ScriptAPI_Application_Quit /EXPORT:ScriptAPI_Application_GetName /EXPORT:ScriptAPI_Application_GetFixedTimeStep /EXPORT:ScriptAPI_Application_IsVSyncEnabled")
    
    set_target_properties(GrapeRuntime PROPERTIES LINK_FLAGS "${SCRIPT_EXPORTS}")
endif()

# ======================= Asset Deployment =========================
# Copy game assets to output directory
# Note: In production, assets should be pre-processed and packaged
add_custom_command(TARGET GrapeRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Deploying game assets..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/../assets"
        "$<TARGET_FILE_DIR:GrapeRuntime>/assets"
    COMMENT "Copy game assets to runtime output directory"
)

message(STATUS "Runtime: Configuration complete")
message(STATUS "  Target: ${GAME_FINAL_OUTPUT_NAME} (Standalone Game)")
message(STATUS "  ImGui: Disabled")
message(STATUS "  Entry Point: ${CMAKE_CURRENT_LIST_DIR}/../src/game/GameMain.cpp")
