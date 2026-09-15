cmake_minimum_required(VERSION 3.22)
project(playground C)

include(FetchContent)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE)

FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3 raylib)

if(ANDROID)
    add_library(combined_demo SHARED src/combined_demo.c)
    target_link_libraries(combined_demo PRIVATE raylib SDL3::SDL3-static
        android log EGL GLESv3)
else()
    add_executable(sdl3_demo src/sdl3_demo.c)
    target_link_libraries(sdl3_demo PRIVATE SDL3::SDL3-static)

    add_executable(raylib_demo src/raylib_demo.c)
    target_link_libraries(raylib_demo PRIVATE raylib)

    add_executable(combined_demo src/combined_demo.c)
    target_link_libraries(combined_demo PRIVATE SDL3::SDL3-static raylib)
endif()
