set(tests_sources)
file(GLOB test_sources "*.cpp")

foreach(tgt ${global_targets})
    add_library(${tgt}_tests STATIC ${test_sources})
    target_link_libraries(${tgt}_tests PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_tests)
endforeach()
