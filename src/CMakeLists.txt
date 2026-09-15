# arak_common: status, logging, config, i18n loader, ring buffers.
# Sprint 1 bootstrap carries only the version/status seed; @code-executor fills the rest.
add_library(arak_common STATIC
    common/version.cpp
)
target_include_directories(arak_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arak_common PUBLIC spdlog::spdlog)
target_compile_definitions(arak_common PUBLIC ARAK_VERSION="${PROJECT_VERSION}")

# airplay-receiver: Sprint 1 window + advertisement land here (brief §6.2).
if(MSVC)
    set(ARAK_EXE_SUBSYSTEM WIN32)
endif()

add_executable(airplay-receiver ${ARAK_EXE_SUBSYSTEM}
    core/app/main.cpp
)
target_link_libraries(airplay-receiver PRIVATE arak_common)
