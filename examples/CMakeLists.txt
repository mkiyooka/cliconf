# サンプルバイナリは build/examples/ 以下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)

# CSV wrapper example
add_executable(example_csv_wrapper
    example_csv_wrapper.cpp
)
target_include_directories(example_csv_wrapper PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(example_csv_wrapper PRIVATE
    csv
    tl::expected
)

# JSON wrapper example
add_executable(example_json_wrapper
    example_json_wrapper.cpp
)
target_include_directories(example_json_wrapper PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(example_json_wrapper PRIVATE
    yyjson
)

# compat layer example (expected + span)
add_executable(example_compat
    example_compat.cpp
)
target_include_directories(example_compat PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(example_compat PRIVATE
    tl::expected
    tcb_span_target
)

