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

# List of external libraries (updated for your dependencies)
set(ALL_LIBS
    PRIVATE glfw
    PRIVATE glad
    PRIVATE imgui
    PRIVATE glm
)

# ======================= SINGLE PROJECT CONFIGURATION =========================
# Since we have a single engine project, not using auto-discovery

# Collect your engine source files
file(GLOB_RECURSE ENGINE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/include/*.h
    ${CMAKE_CURRENT_LIST_DIR}/include/*.hpp
)

# Create executable
add_executable(GrapeEngine ${ENGINE_SOURCES})

# Link libraries
target_link_libraries(GrapeEngine ${ALL_LIBS})

# Include directories
target_include_directories(GrapeEngine PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Set C++ standard for the target
set_property(TARGET GrapeEngine PROPERTY CXX_STANDARD 17)
set_property(TARGET GrapeEngine PROPERTY CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GrapeEngine PRIVATE -Wall -Wextra)
elseif (MSVC)
    target_compile_options(GrapeEngine PRIVATE /W3)
endif()

# Copy assets to build directory (if you have any)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
    add_custom_command(TARGET GrapeEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:GrapeEngine>/assets"
        COMMENT "Copying assets to build directory"
    )
endif()

message(STATUS "GrapeEngine configured successfully!")

# ======================= END CONFIGURATION =========================