cmake_minimum_required(VERSION 3.16)
project(sdlversionshop C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_executable(sdlversionshop
    main.c
    arcade.c
    game.c
    player.c
    map.c
    billiards.c
    barista.c
    audio.c
    input.c
    admin.c
)

target_include_directories(sdlversionshop PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_directories(sdlversionshop PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_IMAGE_LIBRARY_DIRS}
    ${SDL2_MIXER_LIBRARY_DIRS}
    ${SDL2_TTF_LIBRARY_DIRS}
)

target_link_libraries(sdlversionshop PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    m
)

target_compile_options(sdlversionshop PRIVATE
    ${SDL2_CFLAGS_OTHER}
    ${SDL2_IMAGE_CFLAGS_OTHER}
    ${SDL2_MIXER_CFLAGS_OTHER}
    ${SDL2_TTF_CFLAGS_OTHER}
)

file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})
