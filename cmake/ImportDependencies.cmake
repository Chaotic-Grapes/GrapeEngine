include(FetchContent)

# Macro to import GLFW
macro(import_glfw)
    if(NOT TARGET glfw)  # Guard to prevent multiple inclusion
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.3.8
        )
        if(NOT glfw_POPULATED)
            set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            FetchContent_MakeAvailable(glfw)
        endif()
    endif()
endmacro()

# Macro to import glm
macro(import_glm)
    if(NOT TARGET glm)  # Guard to prevent multiple inclusion
        FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG master
        )
        FetchContent_MakeAvailable(glm)
    endif()
endmacro()

# Macro to import glad
macro(import_glad)
    if(NOT TARGET glad)  # Guard to prevent multiple inclusion
        FetchContent_Declare(
            glad
            GIT_REPOSITORY https://github.com/Dav1dde/glad.git
            GIT_TAG c
        )
        FetchContent_MakeAvailable(glad)
        
        add_library(glad STATIC
            ${glad_SOURCE_DIR}/src/glad.c
        )
        target_include_directories(glad PUBLIC 
            ${glad_SOURCE_DIR}/include
        )

        # Add OpenGL dependency for GLAD
        find_package(OpenGL REQUIRED)
        target_link_libraries(glad PUBLIC ${OPENGL_LIBRARIES})
    endif()
endmacro()

# Macro to import Fmod
# Creates interface targets FMOD::Core (always) and FMOD::Studio (if studio/inc exists).
# Uses FMOD_ROOT to locate the SDK (defaults to ${CMAKE_SOURCE_DIR}/lib/Fmod).
macro(import_fmod)
    if (TARGET FMOD::Core)
        return()
    endif()

    set(FMOD_ROOT "${CMAKE_SOURCE_DIR}/lib/Fmod" CACHE PATH "FMOD SDK root")

    # Include dirs
    set(FMOD_CORE_INC   "${FMOD_ROOT}/core/inc")
    set(FMOD_STUDIO_INC "${FMOD_ROOT}/studio/inc")

    # core
    add_library(FMOD_Core INTERFACE)
    add_library(FMOD::Core ALIAS FMOD_Core)
    target_include_directories(FMOD_Core INTERFACE "${FMOD_CORE_INC}")

    # Platform-specific libs
    if (WIN32)
        # Adjust x64/x86 as needed
        set(FMOD_CORE_LIB_DIR "${FMOD_ROOT}/core/lib/windows/x64")
        target_link_directories(FMOD_Core INTERFACE "${FMOD_CORE_LIB_DIR}")
        # Link debug logging lib in Debug, normal in others
        target_link_libraries(FMOD_Core INTERFACE
            $<$<CONFIG:Debug>:fmodL_vc>
            $<$<NOT:$<CONFIG:Debug>>:fmod_vc>)
        # Record the runtime DLL path for the copy helper
        set(FMOD_CORE_DLL_DEBUG   "${FMOD_CORE_LIB_DIR}/fmodL.dll"  CACHE INTERNAL "")
        set(FMOD_CORE_DLL_RELEASE "${FMOD_CORE_LIB_DIR}/fmod.dll"   CACHE INTERNAL "")
    elseif(APPLE)
        set(FMOD_CORE_LIB_DIR "${FMOD_ROOT}/core/lib/osx")
        # On macOS you link the dylib by full path
        target_link_libraries(FMOD_Core INTERFACE
            "$<IF:$<CONFIG:Debug>,${FMOD_CORE_LIB_DIR}/libfmodL.dylib,${FMOD_CORE_LIB_DIR}/libfmod.dylib>")
        set(FMOD_CORE_DYLIB_DEBUG   "${FMOD_CORE_LIB_DIR}/libfmodL.dylib" CACHE INTERNAL "")
        set(FMOD_CORE_DYLIB_RELEASE "${FMOD_CORE_LIB_DIR}/libfmod.dylib"  CACHE INTERNAL "")
    elseif(UNIX)
        set(FMOD_CORE_LIB_DIR "${FMOD_ROOT}/core/lib/linux/x86_64")
        # Prefer absolute libs on Linux
        target_link_libraries(FMOD_Core INTERFACE
            "$<IF:$<CONFIG:Debug>,${FMOD_CORE_LIB_DIR}/libfmodL.so,${FMOD_CORE_LIB_DIR}/libfmod.so>")
        set(FMOD_CORE_SO_DEBUG   "${FMOD_CORE_LIB_DIR}/libfmodL.so" CACHE INTERNAL "")
        set(FMOD_CORE_SO_RELEASE "${FMOD_CORE_LIB_DIR}/libfmod.so"  CACHE INTERNAL "")
    endif()

    # Studio in case 
    if (EXISTS "${FMOD_STUDIO_INC}")
        add_library(FMOD_Studio INTERFACE)
        add_library(FMOD::Studio ALIAS FMOD_Studio)
        target_include_directories(FMOD_Studio INTERFACE "${FMOD_STUDIO_INC}")

        if (WIN32)
            set(FMOD_STUDIO_LIB_DIR "${FMOD_ROOT}/studio/lib/windows/x64")
            target_link_directories(FMOD_Studio INTERFACE "${FMOD_STUDIO_LIB_DIR}")
            target_link_libraries(FMOD_Studio INTERFACE
                $<$<CONFIG:Debug>:fmodstudioL_vc>
                $<$<NOT:$<CONFIG:Debug>>:fmodstudio_vc>)
            set(FMOD_STUDIO_DLL_DEBUG   "${FMOD_STUDIO_LIB_DIR}/fmodstudioL.dll" CACHE INTERNAL "")
            set(FMOD_STUDIO_DLL_RELEASE "${FMOD_STUDIO_LIB_DIR}/fmodstudio.dll"  CACHE INTERNAL "")
        elseif(APPLE)
            set(FMOD_STUDIO_LIB_DIR "${FMOD_ROOT}/studio/lib/osx")
            target_link_libraries(FMOD_Studio INTERFACE
                "$<IF:$<CONFIG:Debug>,${FMOD_STUDIO_LIB_DIR}/libfmodstudioL.dylib,${FMOD_STUDIO_LIB_DIR}/libfmodstudio.dylib>")
            set(FMOD_STUDIO_DYLIB_DEBUG   "${FMOD_STUDIO_LIB_DIR}/libfmodstudioL.dylib" CACHE INTERNAL "")
            set(FMOD_STUDIO_DYLIB_RELEASE "${FMOD_STUDIO_LIB_DIR}/libfmodstudio.dylib"  CACHE INTERNAL "")
        elseif(UNIX)
            set(FMOD_STUDIO_LIB_DIR "${FMOD_ROOT}/studio/lib/linux/x86_64")
            target_link_libraries(FMOD_Studio INTERFACE
                "$<IF:$<CONFIG:Debug>,${FMOD_STUDIO_LIB_DIR}/libfmodstudioL.so,${FMOD_STUDIO_LIB_DIR}/libfmodstudio.so>")
            set(FMOD_STUDIO_SO_DEBUG   "${FMOD_STUDIO_LIB_DIR}/libfmodstudioL.so" CACHE INTERNAL "")
            set(FMOD_STUDIO_SO_RELEASE "${FMOD_STUDIO_LIB_DIR}/libfmodstudio.so"  CACHE INTERNAL "")
        endif()
    endif()
endmacro()

# Helper: copy FMOD runtime next to a target after build
function(fmod_copy_runtime target)
    if (WIN32)
        if (DEFINED FMOD_CORE_DLL_DEBUG AND DEFINED FMOD_CORE_DLL_RELEASE)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<IF:$<CONFIG:Debug>,${FMOD_CORE_DLL_DEBUG},${FMOD_CORE_DLL_RELEASE}>"
                    "$<TARGET_FILE_DIR:${target}>")
        endif()
        if (TARGET FMOD::Studio AND DEFINED FMOD_STUDIO_DLL_DEBUG AND DEFINED FMOD_STUDIO_DLL_RELEASE)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<IF:$<CONFIG:Debug>,${FMOD_STUDIO_DLL_DEBUG},${FMOD_STUDIO_DLL_RELEASE}>"
                    "$<TARGET_FILE_DIR:${target}>")
        endif()
    elseif(APPLE)
        # Optionally copy dylibs into app bundle or set @rpath at install time
        # add_custom_command(...) similar to above if you prefer copying
        message(STATUS "On macOS, prefer setting @rpath or bundle the dylibs.")
    elseif(UNIX)
        # On Linux, prefer rpath or provide a copy rule here if desired
        message(STATUS "On Linux, ensure rpath/LD_LIBRARY_PATH finds the FMOD .so files.")
    endif()
endfunction()


# Macro to import ImGui
macro(import_imgui)
    if(NOT TARGET imgui)  # Guard to prevent multiple inclusion
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.90.4
        )
        if(NOT imgui_POPULATED)
            FetchContent_MakeAvailable(imgui)
        endif()

        # Create ImGui library manually since it doesn't have CMakeLists.txt
        set(IMGUI_SOURCES
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        )

        add_library(imgui STATIC ${IMGUI_SOURCES})
        target_include_directories(imgui PUBLIC 
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
        )

        # Link dependencies; make sure OpenGL is available
        find_package(OpenGL REQUIRED)
        target_link_libraries(imgui PUBLIC 
            glfw 
            glad 
            ${OPENGL_LIBRARIES}
        )
        
        # Set C++ standard for ImGui (required for constexpr support)
        set_property(TARGET imgui PROPERTY CXX_STANDARD 11)
        set_property(TARGET imgui PROPERTY CXX_STANDARD_REQUIRED ON)
    endif()
endmacro()

# Macro to import all dependencies
macro(importDependencies)
    message(STATUS "Starting to import dependencies...")

    message(STATUS "Importing GLFW...")
    import_glfw()
    message(STATUS "GLFW imported successfully.")

    message(STATUS "Importing GLM...")
    import_glm()
    message(STATUS "GLM imported successfully.")

    message(STATUS "Importing GLAD...")
    import_glad()
    message(STATUS "GLAD imported successfully.")

    message(STATUS "Importing ImGui...")
    import_imgui()
    message(STATUS "ImGui imported successfully.")

    message(STATUS "All dependencies have been imported successfully.")
endmacro()