cmake_minimum_required(VERSION 3.28)

project(MeatMonEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# nlohmann/json 3.11.3 declares cmake_minimum_required(3.1); CMake 4.x refuses
# anything below 3.5 unless we raise the floor for fetched dependencies.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(MSVC)
  add_compile_options(/utf-8)
endif()

include(FetchContent)

# ---- Dependencies (all via FetchContent, no system installs) ----------------

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.2.30
  GIT_SHALLOW    TRUE)

FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)

FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(SDL3 nlohmann_json stb)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# ---- Engine modules ----------------------------------------------------------
# battle:  pure C++20 battle simulator, zero SDL/renderer dependencies.
# engine:  window, renderer, fixed-tick loop, assets, tilemap, sprites.
# apps:    the game executable and the headless battle CLI.

enable_testing()

add_subdirectory(battle)
add_subdirectory(engine)
add_subdirectory(apps/game)
add_subdirectory(apps/battle_cli)
add_subdirectory(apps/battle_tests)
