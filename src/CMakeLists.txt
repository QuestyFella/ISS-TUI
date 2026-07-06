add_executable(bloom_valuation main.c)
target_link_libraries(bloom_valuation PRIVATE bloom)

if(MSVC)
    target_compile_options(bloom_valuation PRIVATE /W4)
else()
    target_compile_options(bloom_valuation PRIVATE -Wall -Wextra -pedantic)
endif()

install(TARGETS bloom_valuation
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
