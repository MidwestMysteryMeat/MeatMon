add_library(meatmon_battle STATIC
  src/dex.cpp
  src/battle.cpp
  src/team.cpp)

target_include_directories(meatmon_battle PUBLIC include)
target_link_libraries(meatmon_battle PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(meatmon_battle PUBLIC cxx_std_20)
