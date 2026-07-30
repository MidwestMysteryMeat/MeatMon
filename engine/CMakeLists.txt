add_library(meatmon_engine STATIC
  src/app.cpp
  src/texture.cpp
  src/assets.cpp
  src/tilemap.cpp
  src/sprites.cpp
  src/font.cpp)

target_include_directories(meatmon_engine PUBLIC include)
target_link_libraries(meatmon_engine
  PUBLIC SDL3::SDL3-static nlohmann_json::nlohmann_json
  PRIVATE stb)
target_compile_features(meatmon_engine PUBLIC cxx_std_20)
