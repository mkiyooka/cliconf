# サンプルバイナリは build/examples/ 以下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)

# CSV wrapper example
add_executable(example_csv_wrapper
    example_csv_wrapper.cpp
)
target_link_libraries(example_csv_wrapper PRIVATE
    csv
    tl::expected
)

# JSON wrapper example
add_executable(example_json_wrapper
    example_json_wrapper.cpp
)
target_link_libraries(example_json_wrapper PRIVATE
    yyjson
)

# Output system example (Logger + DataRecorder)
add_executable(example_output
    example_output.cpp
)
target_include_directories(example_output PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(example_output PRIVATE
    fmt::fmt
    spdlog::spdlog
    yyjson
)
