# CMake stuff
cmake_minimum_required(VERSION 3.14) # 3.14+ needed for FetchContent_MakeAvailable
project(Zeyrixon)

include(FetchContent)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Editor/vendor/imgui")

# Export Commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# GLAD is plain C, giving it a small target
add_library(glad STATIC Engine/vendor/glad/src/glad.c)
target_include_directories(glad PUBLIC Engine/vendor/glad/include)
set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Set OpenGL preference policy to suppress warning
cmake_policy(SET CMP0072 NEW)

# find system installed stuff
find_package(OpenGL REQUIRED) # OpenGL comes from the graphics driver, can't be fetched - install mesa/nvidia drivers if missing

# glfw3: use system package if present, otherwise fetch and build from source
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "glfw3 not found on system, fetching from source instead")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()

# Add your files
add_library(imgui_lib STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(Engine STATIC
    Engine/Private/Engine.cpp
    Engine/pch.cpp
    Engine/Private/Base.cpp
    Engine/Private/Components/Common/BaseComponent.cpp
    Engine/Private/Components/Common/Mesh.cpp
    Engine/Private/Components/Common/Model.cpp
    Engine/Private/Components/Common/TransfromComponent.cpp
    Engine/Private/Entities/Common/BaseEntity.cpp
    Engine/Private/Entities/Common/Camera.cpp
    Engine/Private/Entities/Shapes/PrimativeShape.cpp
    Engine/Private/Entities/Shapes/Cube.cpp
    Engine/Private/Entities/Shapes/Sphere.cpp
)

# Assimp stuff: use system package if present, otherwise fetch and build from source
find_package(assimp QUIET)
if(NOT assimp_FOUND)
    message(STATUS "assimp not found on system, fetching from source instead")
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(assimp)
    add_library(assimp::assimp ALIAS assimp)
endif()
target_link_libraries(Engine PUBLIC assimp::assimp)

find_package(pugixml QUIET)
if(NOT pugixml_FOUND)
    message(STATUS "pugixml not found on system, fetching from source instead")
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.14
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pugixml)
endif()

# If you have include folders
target_include_directories(imgui_lib PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

target_include_directories(Engine PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Engine/..)
target_include_directories(Engine PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Engine PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Engine/vendor/stb)
target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Engine/vendor/glm)

add_executable(Editor
    Editor/main.cpp
    Editor/Private/Common/Window.cpp
    Editor/Private/Common/EditorCamera.cpp
    Editor/Private/Utils/PathUtils.cpp
    Editor/Private/Windows/MainWindow.cpp
    Editor/Private/Windows/ProjectBrowserDialog.cpp
    Editor/Private/Utils/FileCreator.cpp
    Editor/Private/Common/Project.cpp
    Editor/Private/Utils/FileManagement.cpp
)
target_include_directories(Editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Editor/..)
target_link_libraries(Editor PRIVATE imgui_lib glad glfw OpenGL::GL Engine pugixml)
if(NOT assimp_FOUND)
    # assimp splices its own bundled copy of pugixml's source directly into
    # specific object files (ImporterRegistry.cpp, ColladaParser.cpp) when
    # built from source, colliding with our separately-built pugixml. Both
    # are the same pugixml version, so it's safe to let the linker keep
    # whichever definition it finds first instead of erroring out.
    target_link_options(Editor PRIVATE -Wl,--allow-multiple-definition)
endif()

target_link_libraries(Engine PUBLIC glad OpenGL::GL)

if(MINGW)
    target_link_options(Editor PRIVATE -static -static-libgcc -static-libstdc++)
endif()
