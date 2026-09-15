cmake_minimum_required(VERSION 3.22)
project(playground C CXX)

include(FetchContent)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE)

FetchContent_Declare(cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)

if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3 cglm)

set(THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

if(ANDROID)
    add_library(main SHARED src/scene_demo.c src/text.c src/ui.c src/perf.c src/third_party_impl.c)
    target_link_libraries(main PRIVATE SDL3::SDL3 cglm
        android log EGL GLESv3)
    target_include_directories(main PRIVATE src ${THIRD_PARTY} ${THIRD_PARTY}/sokol)
else()
    add_executable(sdl3_demo src/sdl3_demo.c)
    target_link_libraries(sdl3_demo PRIVATE SDL3::SDL3-static)

    add_executable(combined_demo src/combined_demo.c src/text.c src/ui.c src/perf.c src/third_party_impl.c)
    target_link_libraries(combined_demo PRIVATE SDL3::SDL3-static cglm
        "-framework OpenGL")
    target_include_directories(combined_demo PRIVATE src ${THIRD_PARTY} ${THIRD_PARTY}/sokol)

    add_executable(scene_demo src/scene_demo.c src/text.c src/third_party_impl.c)
    target_link_libraries(scene_demo PRIVATE SDL3::SDL3-static cglm
        "-framework OpenGL")
    target_include_directories(scene_demo PRIVATE src ${THIRD_PARTY} ${THIRD_PARTY}/sokol)
endif()
