add_executable(mat_tests mat/interval_union.cpp)
target_link_libraries(mat_tests PRIVATE common)
add_test(NAME mat_tests COMMAND mat_tests)