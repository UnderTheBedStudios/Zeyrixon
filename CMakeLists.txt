# CMake stuff
cmake_minimum_required(VERSION 3.10)
project(LumenX)

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
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

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
)

# Assimp stuff
find_package(assimp REQUIRED)
target_link_libraries(Engine PRIVATE assimp::assimp)

# If you have include folders
target_include_directories(imgui_lib PUBLIC 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_include_directories(Engine PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Engine/..)
target_include_directories(Engine PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Engine PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Engine/vendor/stb)
target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Engine/vendor/glm)

add_executable(Editor
    Editor/main.cpp
    Editor/Private/Common/Window.cpp
)
target_include_directories(Editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Editor/..)
target_link_libraries(Editor PRIVATE imgui_lib glad glfw OpenGL::GL Engine)

target_link_libraries(Engine PUBLIC glad OpenGL::GL)
