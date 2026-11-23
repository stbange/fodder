cmake_minimum_required(VERSION 3.24)
project(Fodder VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(EMSCRIPTEN)
  set(CMAKE_C_EXTENSIONS ON)
else()
  set(CMAKE_C_EXTENSIONS OFF)
endif()
include(CTest)
include(FetchContent)
option(FODDER_BUILD_GAME "Build the SDL3/OpenGL game" ON)
option(FODDER_COVERAGE "Instrument first-party code for GCC coverage" OFF)

FetchContent_Declare(fodder_lua
  URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
  URL_HASH SHA256=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
  SOURCE_SUBDIR _fodder)
FetchContent_Declare(fodder_json
  URL https://codeload.github.com/DaveGamble/cJSON/tar.gz/refs/tags/v1.7.19
  URL_HASH SHA256=7fa616e3046edfa7a28a32d5f9eacfd23f92900fe1f8ccd988c1662f30454562
  SOURCE_SUBDIR _fodder)
FetchContent_MakeAvailable(fodder_lua fodder_json)

file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${fodder_lua_SOURCE_DIR}/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/(lua|luac)\\.c$")
add_library(fodder_lua STATIC ${LUA_SOURCES})
target_include_directories(fodder_lua SYSTEM PUBLIC "${fodder_lua_SOURCE_DIR}/src")
if(NOT MSVC)
  target_link_libraries(fodder_lua PUBLIC m)
endif()
if(UNIX AND NOT EMSCRIPTEN)
  target_compile_definitions(fodder_lua PRIVATE LUA_USE_POSIX _POSIX_C_SOURCE=200809L)
endif()
if(BUILD_TESTING AND NOT EMSCRIPTEN)
  add_executable(fodder_lua_runner "${fodder_lua_SOURCE_DIR}/src/lua.c")
  target_link_libraries(fodder_lua_runner PRIVATE fodder_lua)
endif()
add_library(fodder_json STATIC "${fodder_json_SOURCE_DIR}/cJSON.c")
target_include_directories(fodder_json SYSTEM PUBLIC "${fodder_json_SOURCE_DIR}")

function(fodder_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(FODDER_COVERAGE)
    target_compile_options(${target} PRIVATE --coverage -O0 -g)
    target_link_options(${target} PUBLIC --coverage)
  endif()
endfunction()

add_library(fodder_engine STATIC
  src/engine/battle.c src/engine/pack.c src/engine/profile.c
  src/engine/session.c src/engine/geometry.c)
target_include_directories(fodder_engine PUBLIC src/engine)
target_link_libraries(fodder_engine PUBLIC fodder_lua fodder_json)
fodder_warnings(fodder_engine)

if(FODDER_BUILD_GAME)
  set(SDL_SHARED OFF CACHE BOOL "Build shared SDL" FORCE)
  set(SDL_STATIC ON CACHE BOOL "Build static SDL" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test helpers" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "Build SDL tests" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL examples" FORCE)
  FetchContent_Declare(fodder_sdl
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-3.2.24
    URL_HASH SHA256=66c374ee7f786abcd8592d18fc5b18b1e3f1c67e923867eee5f9c503cb253afd)
  FetchContent_MakeAvailable(fodder_sdl)
  add_executable(fodder src/game/main.c src/game/render.c src/game/sound.c)
  if(NOT EMSCRIPTEN)
    target_sources(fodder PRIVATE src/game/profile_store.c)
  endif()
  target_link_libraries(fodder PRIVATE fodder_engine SDL3::SDL3-static)
  fodder_warnings(fodder)
  set_target_properties(fodder PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/play")

  set(FODDER_PACKAGE_DIR "$<TARGET_FILE_DIR:fodder>")
  if(EMSCRIPTEN)
    set_target_properties(fodder PROPERTIES SUFFIX ".mjs")
    target_link_options(fodder PRIVATE
      -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web -sINVOKE_RUN=0
      -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFORCE_FILESYSTEM=1
      -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576
      "-sEXPORTED_RUNTIME_METHODS=['FS','ccall','UTF8ToString','HEAPF32']")
  elseif(APPLE)
    set_target_properties(fodder PROPERTIES
      MACOSX_BUNDLE TRUE MACOSX_BUNDLE_BUNDLE_NAME "Fodder"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.stbange.fodder"
      MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/src/game/Info.plist.in"
      OUTPUT_NAME "Fodder")
    set(FODDER_PACKAGE_DIR "$<TARGET_BUNDLE_DIR:fodder>/..")
  endif()

  set(FODDER_NOTICE_DIR "${CMAKE_BINARY_DIR}/notices")
  file(MAKE_DIRECTORY "${FODDER_NOTICE_DIR}")
  configure_file("${fodder_lua_SOURCE_DIR}/doc/readme.html" "${FODDER_NOTICE_DIR}/lua.html" COPYONLY)
  configure_file("${fodder_json_SOURCE_DIR}/LICENSE" "${FODDER_NOTICE_DIR}/cJSON.txt" COPYONLY)
  configure_file("${fodder_sdl_SOURCE_DIR}/LICENSE.txt" "${FODDER_NOTICE_DIR}/SDL.txt" COPYONLY)
  add_custom_target(fodder_content ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/assets" "${FODDER_PACKAGE_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/LICENSE" "${FODDER_PACKAGE_DIR}/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${FODDER_NOTICE_DIR}" "${FODDER_PACKAGE_DIR}/notices"
    VERBATIM)
  add_dependencies(fodder_content fodder)
  if(EMSCRIPTEN)
    add_custom_command(TARGET fodder_content POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/web" "${FODDER_PACKAGE_DIR}"
      VERBATIM)
  endif()
endif()

if(BUILD_TESTING AND NOT EMSCRIPTEN)
  add_executable(fodder_unit tests/unit/core.c)
  target_link_libraries(fodder_unit PRIVATE fodder_engine)
  fodder_warnings(fodder_unit)
  add_test(NAME unit_core COMMAND fodder_unit "${PROJECT_SOURCE_DIR}/assets/packs/fantasy")
  set_tests_properties(unit_core PROPERTIES LABELS unit)
  add_test(NAME unit_fantasy_lua COMMAND fodder_lua_runner
    "${PROJECT_SOURCE_DIR}/tests/unit/rules.lua"
    "${PROJECT_SOURCE_DIR}/assets/packs/fantasy/scripts/battles.lua")
  set_tests_properties(unit_fantasy_lua PROPERTIES LABELS unit)
  add_executable(fodder_scenarios tests/game/scenarios.c)
  target_link_libraries(fodder_scenarios PRIVATE fodder_engine)
  fodder_warnings(fodder_scenarios)
  add_test(NAME game_scenarios COMMAND fodder_scenarios "${PROJECT_SOURCE_DIR}/assets/packs/fantasy")
  set_tests_properties(game_scenarios PROPERTIES LABELS game TIMEOUT 100)
  if(FODDER_BUILD_GAME)
    add_test(NAME native_launch COMMAND fodder --smoke)
    set_tests_properties(native_launch PROPERTIES
      WORKING_DIRECTORY "$<TARGET_FILE_DIR:fodder>" LABELS display TIMEOUT 30)
    if(FODDER_COVERAGE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
      set_tests_properties(native_launch PROPERTIES LABELS "unit;display"
        ENVIRONMENT "XDG_DATA_HOME=${CMAKE_BINARY_DIR}/unit-smoke;SDL_AUDIODRIVER=dummy")
      add_executable(fodder_platform_unit tests/unit/native.c
        src/game/profile_store.c src/game/render.c src/game/sound.c)
      target_include_directories(fodder_platform_unit PRIVATE src/game)
      target_link_libraries(fodder_platform_unit PRIVATE fodder_engine SDL3::SDL3-static)
      fodder_warnings(fodder_platform_unit)
      add_test(NAME unit_platform COMMAND fodder_platform_unit "$<TARGET_FILE_DIR:fodder>/assets")
      set_tests_properties(unit_platform PROPERTIES LABELS "unit;display" TIMEOUT 30
        ENVIRONMENT "XDG_DATA_HOME=${CMAKE_BINARY_DIR}/unit-data;SDL_AUDIODRIVER=dummy")
    endif()

  endif()
endif()
