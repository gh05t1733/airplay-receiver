find_package(Catch2 3 CONFIG REQUIRED)

add_executable(arak_tests
    unit/test_version.cpp
)
target_link_libraries(arak_tests PRIVATE arak_common Catch2::Catch2WithMain)

add_test(NAME arak_tests COMMAND arak_tests)
