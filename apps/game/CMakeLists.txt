add_executable(meatmon
  main.cpp
  battle_scene.cpp)
target_link_libraries(meatmon PRIVATE meatmon_engine meatmon_battle)
