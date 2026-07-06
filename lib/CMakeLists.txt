add_library(bloom STATIC bloom.c)

target_include_directories(bloom PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
    target_compile_options(bloom PRIVATE /W4)
else()
    target_compile_options(bloom PRIVATE -Wall -Wextra -pedantic)
endif()

install(TARGETS bloom
    EXPORT bloomTargets
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES bloom.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT bloomTargets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bloom
    NAMESPACE bloom::
)
