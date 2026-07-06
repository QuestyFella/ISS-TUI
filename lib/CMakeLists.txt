add_library(iss_tui_lib STATIC iss_tui.c)

target_include_directories(iss_tui_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
    target_compile_options(iss_tui_lib PRIVATE /W4)
else()
    target_compile_options(iss_tui_lib PRIVATE -Wall -Wextra -pedantic)
endif()

install(TARGETS iss_tui_lib
    EXPORT iss_tuiTargets
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES iss_tui.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT iss_tuiTargets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iss_tui
    NAMESPACE iss_tui::
)
