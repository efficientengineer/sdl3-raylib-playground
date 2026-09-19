cmake_minimum_required(VERSION 3.22)
project(playground C CXX)

include(FetchContent)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE)

if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    # 16KB page alignment for all shared libraries
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3)

set(THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set(IMGUI_DIR ${THIRD_PARTY}/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(ANDROID)
    # ImGui as shared lib — single copy of ImGui state
    add_library(imgui_shared SHARED ${IMGUI_SOURCES})
    target_include_directories(imgui_shared PUBLIC
        ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(imgui_shared PUBLIC SDL3::SDL3 EGL GLESv3 android log)
    target_compile_definitions(imgui_shared PUBLIC IMGUI_IMPL_OPENGL_ES3)

    # Game logic: hot-reloadable
    # The game. src/game_logic.cpp is the previous RPG, kept for reference and no longer built.
    add_library(game_logic SHARED src/star_logic.cpp src/field.cpp src/tilefield.cpp)
    target_include_directories(game_logic PRIVATE src ${THIRD_PARTY}
        ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(game_logic PRIVATE imgui_shared)
    set_target_properties(game_logic PROPERTIES
        CXX_VISIBILITY_PRESET hidden)

    # Host: entry point, owns window/GL/ImGui init, hot-reload machinery
    add_library(main SHARED src/host.cpp)
    target_link_libraries(main PRIVATE imgui_shared android log EGL GLESv3)
    target_include_directories(main PRIVATE src ${THIRD_PARTY}
        ${IMGUI_DIR} ${IMGUI_DIR}/backends)
else()
    add_library(imgui_shared SHARED ${IMGUI_SOURCES})
    target_include_directories(imgui_shared PUBLIC
        ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(imgui_shared PUBLIC SDL3::SDL3-static "-framework OpenGL")

    # The game. src/game_logic.cpp is the previous RPG, kept for reference and no longer built.
    add_library(game_logic SHARED src/star_logic.cpp src/field.cpp src/tilefield.cpp)
    target_include_directories(game_logic PRIVATE src ${THIRD_PARTY}
        ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(game_logic PRIVATE imgui_shared)
    set_target_properties(game_logic PROPERTIES
        CXX_VISIBILITY_PRESET hidden)

    add_executable(quest_glory src/host.cpp)
    target_link_libraries(quest_glory PRIVATE imgui_shared)
    target_include_directories(quest_glory PRIVATE src ${THIRD_PARTY}
        ${IMGUI_DIR} ${IMGUI_DIR}/backends)

    add_executable(scene_demo src/scene_demo.cpp ${IMGUI_SOURCES})
    target_link_libraries(scene_demo PRIVATE SDL3::SDL3-static
        "-framework OpenGL")
    target_include_directories(scene_demo PRIVATE src ${THIRD_PARTY}
        ${IMGUI_DIR} ${IMGUI_DIR}/backends)
endif()
