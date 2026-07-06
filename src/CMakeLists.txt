add_executable(iss_tui main.c)
target_link_libraries(iss_tui PRIVATE iss_tui_lib)

if(MSVC)
    target_compile_options(iss_tui PRIVATE /W4)
else()
    target_compile_options(iss_tui PRIVATE -Wall -Wextra -pedantic)
endif()

install(TARGETS iss_tui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
