# ======================= Editor Executable Configuration =========================
# Editor is a C++ executable that provides level editing, ImGui UI, and tools.
# It links against the GrapeEngineLib and adds editor-specific functionality.
#
# Dependencies:
# - GrapeEngineLib (engine core)
# - ImGui (editor UI)
# - ImGuizmo (transform gizmos)
# ==================================================================================

cmake_minimum_required(VERSION 3.13)

project(GrapeEditor VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ======================= Collect Editor Source Files =========================
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c
)

file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/include/*.h
    ${CMAKE_CURRENT_LIST_DIR}/include/*.hpp
)

# ======================= Create Editor Executable =========================
add_executable(GrapeEditor 
    ${EDITOR_SOURCES} 
    ${EDITOR_HEADERS}
    # Force global new/delete override to be compiled into the Editor EXE
    ../engine/src/services/GlobalMemoryOverride.cpp
    # Explicitly include new editor panels that may not be picked up yet by glob verification.
    ${CMAKE_CURRENT_LIST_DIR}/src/GameConfigPanel.cpp
)

# Set output name
set_target_properties(GrapeEditor PROPERTIES OUTPUT_NAME "GrapeEngine")

# Make Release builds a GUI/Windows app (no console)
# Keep Debug builds as console apps for developer logging
set_target_properties(GrapeEditor PROPERTIES 
    WIN32_EXECUTABLE "$<$<CONFIG:Release>:TRUE>"
)

# Set Visual Studio debugger working directory to build output
# This ensures assets and config files are found when debugging via .sln
set_target_properties(GrapeEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:GrapeEditor>"
)

# ======================= Embed Icon (Windows) =========================
set(EDITOR_ICON "${CMAKE_CURRENT_LIST_DIR}/../assets/editor_icon.ico" CACHE FILEPATH "Icon file for the editor executable")

if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/../app_editor.rc.in" 
        "${CMAKE_BINARY_DIR}/app_editor.rc" 
        @ONLY
    )
    target_sources(GrapeEditor PRIVATE "${CMAKE_BINARY_DIR}/app_editor.rc")
    
    if(NOT EXISTS "${EDITOR_ICON}")
        message(WARNING "Editor icon not found: ${EDITOR_ICON}")
    endif()
endif()

# ======================= Include Directories =========================
target_include_directories(GrapeEditor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# ======================= Precompiled Headers =========================
set(GRAPEENGINE_PCH_HEADER "${CMAKE_CURRENT_LIST_DIR}/../engine/include/pch.h")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16 AND EXISTS "${GRAPEENGINE_PCH_HEADER}")
    target_precompile_headers(GrapeEditor PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${GRAPEENGINE_PCH_HEADER}>"
    )
else()
    message(STATUS "Editor: Precompiled headers disabled (CMake >= 3.16 required)")
endif()

# ======================= Compiler Definitions =========================
# USE_IMGUI enables ImGui code paths (editor-only)
target_compile_definitions(GrapeEditor PRIVATE USE_IMGUI)

# ======================= Link Dependencies =========================
# Link against the engine library
target_link_libraries(GrapeEditor
    PRIVATE
        GrapeEngineLib
        imgui
)

# Add editor-only external sources (excluded from engine lib)
target_sources(GrapeEditor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../externals/src/ImGuizmo.cpp
)

# ======================= Script API Exports =========================
# Export C# P/Invoke functions from editor executable
if(MSVC AND NOT BUILD_ENGINE_SHARED)
    set(SCRIPT_EXPORTS "/EXPORT:ScriptAPI_GetComponent /EXPORT:ScriptAPI_GetComponentPtr /EXPORT:ScriptAPI_AddComponent /EXPORT:ScriptAPI_SetComponent /EXPORT:ScriptAPI_HasComponent /EXPORT:ScriptAPI_RemoveComponent /EXPORT:ScriptAPI_CreateEntity /EXPORT:ScriptAPI_DestroyEntity /EXPORT:ScriptAPI_IsAlive /EXPORT:ScriptAPI_SetWorld /EXPORT:ScriptAPI_IsKeyPressed /EXPORT:ScriptAPI_IsKeyDown /EXPORT:ScriptAPI_IsKeyUp /EXPORT:ScriptAPI_IsMousePressed /EXPORT:ScriptAPI_GetMouseX /EXPORT:ScriptAPI_GetMouseY /EXPORT:ScriptAPI_GetScrollX /EXPORT:ScriptAPI_GetScrollY /EXPORT:ScriptAPI_Time_GetDeltaTime /EXPORT:ScriptAPI_Time_GetUnscaledDeltaTime /EXPORT:ScriptAPI_Time_GetFixedDeltaTime /EXPORT:ScriptAPI_Time_GetUnscaledFixedDeltaTime /EXPORT:ScriptAPI_Time_GetElapsedTime /EXPORT:ScriptAPI_Time_GetFrameCount /EXPORT:ScriptAPI_Time_GetTimeScale /EXPORT:ScriptAPI_Time_SetTimeScale /EXPORT:ScriptAPI_Time_GetMaximumDeltaTime /EXPORT:ScriptAPI_Time_SetMaximumDeltaTime /EXPORT:ScriptAPI_Audio_LoadCue /EXPORT:ScriptAPI_Audio_UnloadCue /EXPORT:ScriptAPI_Audio_HasCue /EXPORT:ScriptAPI_Audio_Play /EXPORT:ScriptAPI_Audio_PlaySingle /EXPORT:ScriptAPI_Audio_Stop /EXPORT:ScriptAPI_Audio_StopCue /EXPORT:ScriptAPI_Audio_IsCuePlaying /EXPORT:ScriptAPI_Audio_SetInstanceVolume /EXPORT:ScriptAPI_Audio_SetInstancePitch /EXPORT:ScriptAPI_Audio_SetInstancePosition /EXPORT:ScriptAPI_Audio_SetMasterVolume /EXPORT:ScriptAPI_Audio_GetMasterVolume /EXPORT:ScriptAPI_Audio_SetListener /EXPORT:ScriptAPI_Physics_SetGravity /EXPORT:ScriptAPI_Physics_GetGravity /EXPORT:ScriptAPI_Physics_SetEnabled /EXPORT:ScriptAPI_Physics_IsEnabled /EXPORT:ScriptAPI_Physics_ApplyForce /EXPORT:ScriptAPI_Physics_ApplyImpulse /EXPORT:ScriptAPI_Physics_GetVelocity /EXPORT:ScriptAPI_Physics_SetVelocity /EXPORT:ScriptAPI_Math_RandomInt /EXPORT:ScriptAPI_Math_RandomFloat /EXPORT:ScriptAPI_Math_RandomIntSeeded /EXPORT:ScriptAPI_Math_RandomFloatSeeded /EXPORT:ScriptAPI_Math_Clamp /EXPORT:ScriptAPI_Math_ClampInt /EXPORT:ScriptAPI_Math_Lerp /EXPORT:ScriptAPI_Math_Abs /EXPORT:ScriptAPI_Math_Sqrt /EXPORT:ScriptAPI_Math_Pow /EXPORT:ScriptAPI_Math_Round /EXPORT:ScriptAPI_Math_Floor /EXPORT:ScriptAPI_Math_Ceil /EXPORT:ScriptAPI_Math_Min /EXPORT:ScriptAPI_Math_Max /EXPORT:ScriptAPI_Math_Sin /EXPORT:ScriptAPI_Math_Cos /EXPORT:ScriptAPI_Math_Tan /EXPORT:ScriptAPI_Math_Asin /EXPORT:ScriptAPI_Math_Acos /EXPORT:ScriptAPI_Math_Atan /EXPORT:ScriptAPI_Math_Atan2 /EXPORT:ScriptAPI_Math_DegToRad /EXPORT:ScriptAPI_Math_RadToDeg /EXPORT:ScriptAPI_Math_Distance2D /EXPORT:ScriptAPI_Math_DistanceSquared2D /EXPORT:ScriptAPI_Math_Length2D /EXPORT:ScriptAPI_Math_LengthSquared2D /EXPORT:ScriptAPI_Math_Normalize2D /EXPORT:ScriptAPI_Math_Dot2D /EXPORT:ScriptAPI_Math_Cross2D /EXPORT:ScriptAPI_Math_Lerp2D /EXPORT:ScriptAPI_Math_ClampVector2D /EXPORT:ScriptAPI_Math_AngleTo /EXPORT:ScriptAPI_Math_DirectionFromAngle /EXPORT:ScriptAPI_Math_Rotate2D /EXPORT:ScriptAPI_Debug_LogInfo /EXPORT:ScriptAPI_Debug_LogDebug /EXPORT:ScriptAPI_Debug_LogWarning /EXPORT:ScriptAPI_Debug_LogError /EXPORT:ScriptAPI_Window_GetWidth /EXPORT:ScriptAPI_Window_GetHeight /EXPORT:ScriptAPI_Window_ShouldClose /EXPORT:ScriptAPI_Window_Close /EXPORT:ScriptAPI_Window_IsFocused /EXPORT:ScriptAPI_Window_IsMinimized /EXPORT:ScriptAPI_Window_SetMinimized /EXPORT:ScriptAPI_Window_IsMaximized /EXPORT:ScriptAPI_Window_SetMaximized /EXPORT:ScriptAPI_Window_IsVisible /EXPORT:ScriptAPI_Window_SetVisible /EXPORT:ScriptAPI_Window_IsResizable /EXPORT:ScriptAPI_Window_SetResizable /EXPORT:ScriptAPI_Window_SetMode /EXPORT:ScriptAPI_Window_HasMode /EXPORT:ScriptAPI_Window_Resize /EXPORT:ScriptAPI_Application_Quit /EXPORT:ScriptAPI_Application_GetName /EXPORT:ScriptAPI_Application_GetFixedTimeStep /EXPORT:ScriptAPI_Application_IsVSyncEnabled")
    
    set_target_properties(GrapeEditor PROPERTIES LINK_FLAGS "${SCRIPT_EXPORTS}")
endif()

# ======================= IDE Organization - Visual Studio Filters =========================
# Organize editor source files by folder structure
foreach(source IN LISTS EDITOR_SOURCES)
    get_filename_component(source_path "${source}" PATH)
    file(RELATIVE_PATH source_path_rel "${CMAKE_CURRENT_LIST_DIR}/src" "${source_path}")
    if(NOT source_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Editor Source\\${source_path_rel}")
    else()
        set(group_path "Editor Source")
    endif()
    source_group("${group_path}" FILES "${source}")
endforeach()

# Organize editor header files by folder structure
foreach(header IN LISTS EDITOR_HEADERS)
    get_filename_component(header_path "${header}" PATH)
    file(RELATIVE_PATH header_path_rel "${CMAKE_CURRENT_LIST_DIR}/include" "${header_path}")
    if(NOT header_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path "Editor Include\\${header_path_rel}")
    else()
        set(group_path "Editor Include")
    endif()
    source_group("${group_path}" FILES "${header}")
endforeach()

message(STATUS "Editor: Configuration complete")
message(STATUS "  Target: GrapeEngine (Editor)")
message(STATUS "  ImGui: Enabled")
message(STATUS "  Sources: ${CMAKE_CURRENT_LIST_DIR}/src")
