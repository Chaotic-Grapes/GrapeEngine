# ======================= Engine Library Configuration =========================
# Engine is a standalone C++ library containing the core ECS, rendering, physics,
# and systems. It has NO dependencies on Editor code or ImGui.
#
# This library can be:
# - Linked by the Editor executable
# - Linked by the Runtime/Game executable
# - Wrapped by EngineInterop for C# scripting
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
    
    # Define export macro for shared library
    target_compile_definitions(GrapeEngineLib PRIVATE GRAPEENGINE_EXPORTS)
    
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

# ======================= Script API Exports (Shared Library) =========================
# When building as shared library, export C# P/Invoke functions from the DLL
if(BUILD_ENGINE_SHARED AND MSVC)
    set(SCRIPT_EXPORTS "/EXPORT:ScriptAPI_GetComponent /EXPORT:ScriptAPI_GetComponentPtr /EXPORT:ScriptAPI_AddComponent /EXPORT:ScriptAPI_SetComponent /EXPORT:ScriptAPI_HasComponent /EXPORT:ScriptAPI_RemoveComponent /EXPORT:ScriptAPI_CreateEntity /EXPORT:ScriptAPI_DestroyEntity /EXPORT:ScriptAPI_IsAlive /EXPORT:ScriptAPI_SetWorld /EXPORT:ScriptAPI_IsKeyPressed /EXPORT:ScriptAPI_IsKeyDown /EXPORT:ScriptAPI_IsKeyUp /EXPORT:ScriptAPI_IsMousePressed /EXPORT:ScriptAPI_GetMouseX /EXPORT:ScriptAPI_GetMouseY /EXPORT:ScriptAPI_GetScrollX /EXPORT:ScriptAPI_GetScrollY /EXPORT:ScriptAPI_Time_GetDeltaTime /EXPORT:ScriptAPI_Time_GetUnscaledDeltaTime /EXPORT:ScriptAPI_Time_GetFixedDeltaTime /EXPORT:ScriptAPI_Time_GetUnscaledFixedDeltaTime /EXPORT:ScriptAPI_Time_GetElapsedTime /EXPORT:ScriptAPI_Time_GetFrameCount /EXPORT:ScriptAPI_Time_GetTimeScale /EXPORT:ScriptAPI_Time_SetTimeScale /EXPORT:ScriptAPI_Time_GetMaximumDeltaTime /EXPORT:ScriptAPI_Time_SetMaximumDeltaTime /EXPORT:ScriptAPI_Audio_LoadCue /EXPORT:ScriptAPI_Audio_UnloadCue /EXPORT:ScriptAPI_Audio_HasCue /EXPORT:ScriptAPI_Audio_Play /EXPORT:ScriptAPI_Audio_PlaySingle /EXPORT:ScriptAPI_Audio_Stop /EXPORT:ScriptAPI_Audio_StopCue /EXPORT:ScriptAPI_Audio_IsCuePlaying /EXPORT:ScriptAPI_Audio_SetInstanceVolume /EXPORT:ScriptAPI_Audio_SetInstancePitch /EXPORT:ScriptAPI_Audio_SetInstancePosition /EXPORT:ScriptAPI_Audio_SetMasterVolume /EXPORT:ScriptAPI_Audio_GetMasterVolume /EXPORT:ScriptAPI_Audio_SetListener /EXPORT:ScriptAPI_Physics_SetGravity /EXPORT:ScriptAPI_Physics_GetGravity /EXPORT:ScriptAPI_Physics_SetEnabled /EXPORT:ScriptAPI_Physics_IsEnabled /EXPORT:ScriptAPI_Physics_ApplyForce /EXPORT:ScriptAPI_Physics_ApplyImpulse /EXPORT:ScriptAPI_Physics_GetVelocity /EXPORT:ScriptAPI_Physics_SetVelocity /EXPORT:ScriptAPI_UIEvents_Clear /EXPORT:ScriptAPI_UIEvents_WasClicked /EXPORT:ScriptAPI_UIEvents_WasHoverEntered /EXPORT:ScriptAPI_UIEvents_WasHoverExited /EXPORT:ScriptAPI_UIEvents_GetEventCount /EXPORT:ScriptAPI_UIEvents_GetEvent /EXPORT:ScriptAPI_UIEvents_GetHoveredEntity /EXPORT:ScriptAPI_UIEvents_IsMouseOverUI /EXPORT:ScriptAPI_Math_RandomInt /EXPORT:ScriptAPI_Math_RandomFloat /EXPORT:ScriptAPI_Math_RandomIntSeeded /EXPORT:ScriptAPI_Math_RandomFloatSeeded /EXPORT:ScriptAPI_Math_Clamp /EXPORT:ScriptAPI_Math_ClampInt /EXPORT:ScriptAPI_Math_Lerp /EXPORT:ScriptAPI_Math_Abs /EXPORT:ScriptAPI_Math_Sqrt /EXPORT:ScriptAPI_Math_Pow /EXPORT:ScriptAPI_Math_Round /EXPORT:ScriptAPI_Math_Floor /EXPORT:ScriptAPI_Math_Ceil /EXPORT:ScriptAPI_Math_Min /EXPORT:ScriptAPI_Math_Max /EXPORT:ScriptAPI_Math_Sin /EXPORT:ScriptAPI_Math_Cos /EXPORT:ScriptAPI_Math_Tan /EXPORT:ScriptAPI_Math_Asin /EXPORT:ScriptAPI_Math_Acos /EXPORT:ScriptAPI_Math_Atan /EXPORT:ScriptAPI_Math_Atan2 /EXPORT:ScriptAPI_Math_DegToRad /EXPORT:ScriptAPI_Math_RadToDeg /EXPORT:ScriptAPI_Math_Distance2D /EXPORT:ScriptAPI_Math_DistanceSquared2D /EXPORT:ScriptAPI_Math_Length2D /EXPORT:ScriptAPI_Math_LengthSquared2D /EXPORT:ScriptAPI_Math_Normalize2D /EXPORT:ScriptAPI_Math_Dot2D /EXPORT:ScriptAPI_Math_Cross2D /EXPORT:ScriptAPI_Math_Lerp2D /EXPORT:ScriptAPI_Math_ClampVector2D /EXPORT:ScriptAPI_Math_AngleTo /EXPORT:ScriptAPI_Math_DirectionFromAngle /EXPORT:ScriptAPI_Math_Rotate2D /EXPORT:ScriptAPI_Debug_LogInfo /EXPORT:ScriptAPI_Debug_LogDebug /EXPORT:ScriptAPI_Debug_LogWarning /EXPORT:ScriptAPI_Debug_LogError /EXPORT:ScriptAPI_Window_GetWidth /EXPORT:ScriptAPI_Window_GetHeight /EXPORT:ScriptAPI_Window_ShouldClose /EXPORT:ScriptAPI_Window_Close /EXPORT:ScriptAPI_Window_IsFocused /EXPORT:ScriptAPI_Window_IsMinimized /EXPORT:ScriptAPI_Window_SetMinimized /EXPORT:ScriptAPI_Window_IsMaximized /EXPORT:ScriptAPI_Window_SetMaximized /EXPORT:ScriptAPI_Window_IsVisible /EXPORT:ScriptAPI_Window_SetVisible /EXPORT:ScriptAPI_Window_IsResizable /EXPORT:ScriptAPI_Window_SetResizable /EXPORT:ScriptAPI_Window_SetMode /EXPORT:ScriptAPI_Window_HasMode /EXPORT:ScriptAPI_Window_Resize /EXPORT:ScriptAPI_Application_Quit /EXPORT:ScriptAPI_Application_GetName /EXPORT:ScriptAPI_Application_GetFixedTimeStep /EXPORT:ScriptAPI_Application_IsVSyncEnabled")
    
    set_target_properties(GrapeEngineLib PROPERTIES LINK_FLAGS "${SCRIPT_EXPORTS}")
    message(STATUS "Engine: Exporting Script API symbols from DLL")
endif()

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
