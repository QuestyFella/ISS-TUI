add_executable(test_iss_tui test_iss_tui.c)
target_link_libraries(test_iss_tui PRIVATE iss_tui_lib)

add_test(NAME unit_iss_tui_name COMMAND test_iss_tui)
add_test(NAME smoke COMMAND iss_tui)
