find_package(Catch2 3 CONFIG REQUIRED)

add_executable(arak_tests
    unit/test_version.cpp
    unit/test_bplist.cpp
    unit/test_features.cpp
    unit/test_identity.cpp
)
target_link_libraries(arak_tests PRIVATE
    arak_common
    arak_rtsp
    Catch2::Catch2WithMain
)

add_test(NAME arak_tests COMMAND arak_tests)
add_executable(arak_bplist_debug
    unit/test_bplist_debug.cpp
)
target_link_libraries(arak_bplist_debug PRIVATE arak_rtsp Catch2::Catch2WithMain)
add_test(NAME arak_bplist_debug COMMAND arak_bplist_debug -c "[bplist]")
