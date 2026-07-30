add_executable(battle_tests main.cpp)
target_link_libraries(battle_tests PRIVATE meatmon_battle)

add_test(NAME battle_tests
         COMMAND battle_tests --data ${CMAKE_SOURCE_DIR}/game/data)
