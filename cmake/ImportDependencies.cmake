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

# Macro to import ImGui
macro(import_imgui)
    if(NOT TARGET imgui)  # Guard to prevent multiple inclusion
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG docking
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

# Macro to import FreeType
macro(import_freetype) 
    if(NOT TARGET freetype)  # Guard to prevent multiple inclusion
        FetchContent_Declare(
            freetype
            GIT_REPOSITORY https://github.com/freetype/freetype.git
            GIT_TAG VER-2-14-1
        )

        if(NOT freetype_POPULATED) 
            # Disable unnecessary FreeType features
            # We only need .ttf, .otf for basic font rendering
            set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
            set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
            set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
            set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
            set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
            
            FetchContent_MakeAvailable(freetype)
        endif()
    endif()
endmacro()

# Macro to import FMOD
macro(import_fmod)
    if(NOT TARGET fmod) # Guard to prevent multiple inclusion
        set(FMOD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/externals/fmod")
        set(FMOD_LIB_DIR "${CMAKE_SOURCE_DIR}/lib/fmod")

        # Create an imported library target
        add_library(fmod UNKNOWN IMPORTED)
        set_target_properties(fmod PROPERTIES
            IMPORTED_LOCATION "${FMOD_LIB_DIR}/fmod_vc.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"
        )
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

    message(STATUS "Importing FreeType...")
    import_freetype()
    message(STATUS "FreeType imported successfully.")

    message(STATUS "Importing FMOD...")
    import_fmod()
    message(STATUS "FMOD imported successfully.")

    message(STATUS "All dependencies have been imported successfully.")
endmacro()