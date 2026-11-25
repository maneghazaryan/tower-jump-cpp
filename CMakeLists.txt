cmake_minimum_required(VERSION 3.20)
project(TowerJump VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(TJ_BUILD_GAME  "Build the raylib game"           ON)
option(TJ_BUILD_TESTS "Build unit tests and simulator"  ON)

# CMake 4 rejects dependencies that still declare cmake_minimum_required(< 3.5), e.g. doctest.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(json)

# ---- core: the simulation, no graphics -------------------------------------------------------
add_library(tj_core
  src/core/config.cpp
  src/core/tower.cpp
  src/core/game.cpp)
target_include_directories(tj_core PUBLIC src)
target_link_libraries(tj_core PUBLIC nlohmann_json::nlohmann_json)
if(MSVC)
  target_compile_options(tj_core PRIVATE /W4)
else()
  target_compile_options(tj_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

# ---- game ------------------------------------------------------------------------------------
if(TJ_BUILD_GAME)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(raylib)

  add_executable(tower_jump src/game/main.cpp src/game/app.cpp)
  target_link_libraries(tower_jump PRIVATE tj_core raylib)

  if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_link_options(tower_jump PRIVATE
      -sUSE_GLFW=3 -sGL_ENABLE_GET_PROC_ADDRESS=1 -sALLOW_MEMORY_GROWTH=1
      --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
      --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html)
    set_target_properties(tower_jump PROPERTIES OUTPUT_NAME "index")
  else()
    add_custom_command(TARGET tower_jump POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:tower_jump>/assets)
  endif()
endif()

# ---- tests -----------------------------------------------------------------------------------
if(TJ_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(doctest)

  add_executable(tj_tests tests/test_main.cpp tests/test_ecs.cpp tests/test_config.cpp tests/test_tower.cpp tests/test_physics.cpp)
  target_link_libraries(tj_tests PRIVATE tj_core doctest::doctest)
  target_compile_definitions(tj_tests PRIVATE TJ_ASSETS="${CMAKE_SOURCE_DIR}/assets/")
  add_test(NAME tj_tests COMMAND tj_tests)

  # Headless simulator: plays the game with a scripted bot and prints stats.
  add_executable(tj_sim tools/sim.cpp)
  target_link_libraries(tj_sim PRIVATE tj_core)
  target_compile_definitions(tj_sim PRIVATE TJ_ASSETS="${CMAKE_SOURCE_DIR}/assets/")
endif()
