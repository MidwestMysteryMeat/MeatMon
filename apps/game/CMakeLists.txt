add_executable(meatmon
  main.cpp
  battle_scene.cpp
  editor_ui.cpp)
target_link_libraries(meatmon PRIVATE meatmon_engine meatmon_battle imgui stb)
