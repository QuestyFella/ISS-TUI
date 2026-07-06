add_executable(test_bloom test_bloom.c)
target_link_libraries(test_bloom PRIVATE bloom)

add_test(NAME unit_bloom_name COMMAND test_bloom)
add_test(NAME smoke COMMAND bloom_valuation)
