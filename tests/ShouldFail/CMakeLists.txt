add_library(payload_data-baseline PayloadData.cpp)
target_link_libraries(payload_data-baseline PRIVATE nvtx3-cpp)

function(add_should_fail_test SOURCE_FILE COMPILE_DEFINITION)
    # Creates a target given a source file and a compile definition
    # that should fail to compile
    # The target is excluded from regular builds, but is built specifically
    # by a new test that is expected to fail.
    string(TOLOWER "${COMPILE_DEFINITION}" TARGET_NAME)
    set(TEST_NAME "ShouldFail ${TARGET_NAME}")
    add_library(${TARGET_NAME} ${SOURCE_FILE})
    target_link_libraries(${TARGET_NAME} PRIVATE nvtx3-cpp)

    set_target_properties(
        ${TARGET_NAME} PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
    target_compile_definitions(
        ${TARGET_NAME}
        PRIVATE
        ${COMPILE_DEFINITION}
    )

    add_test(
        NAME ${TEST_NAME}
        COMMAND ${CMAKE_COMMAND} --build . --target ${TARGET_NAME} --config $<CONFIGURATION>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    set_tests_properties(${TEST_NAME} PROPERTIES WILL_FAIL TRUE)
endfunction()

add_test(
    NAME "ShouldFail payload_data-baseline"
    COMMAND ${CMAKE_COMMAND} --build . --target payload_data-baseline --config $<CONFIGURATION>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_should_fail_test(
    PayloadData.cpp
    EVENT_ATTRIBUTES_FROM_PAYLOAD_DATA_FROM_TEMPORARY
)

add_should_fail_test(
    PayloadData.cpp
    SCOPED_RANGE_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
)

add_should_fail_test(
    PayloadData.cpp
    START_RANGE_IN_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
)

add_should_fail_test(
    PayloadData.cpp
    START_RANGE_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
)

add_should_fail_test(
    PayloadData.cpp
    SCOPE_FRANGE_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_ARRAY_OF_PAYLOAD_DATA
)

add_should_fail_test(
    PayloadData.cpp
    START_RANGE_IN_FROM_PAYLOAD_DATA_FROM_TEMPORARY
)

add_should_fail_test(
    PayloadData.cpp
    START_RANGE_FROM_PAYLOAD_DATA_FROM_TEMPORARY
)

add_should_fail_test(
    PayloadData.cpp
    EVENT_ATTRIBUTES_FROM_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
)

add_should_fail_test(
    PayloadData.cpp
    EVENT_ATTRIBUTES_COPY_EVENT_ATTRIBUTES_FROM_TEMPORARY_PAYLOAD_DATA
)


add_library(message-baseline Message.cpp)
target_link_libraries(message-baseline PRIVATE nvtx3-cpp)

add_test(
    NAME "ShouldFail message-baseline"
    COMMAND ${CMAKE_COMMAND} --build . --target message-baseline --config $<CONFIGURATION>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add tests that should fail to compile
add_should_fail_test(
    Message.cpp
    EVENT_ATTRIBUTES_FROM_MESSAGE_FROM_TEMPORARY_STRING
)

add_should_fail_test(
    Message.cpp
    START_RANGE_IN_FROM_MESSAGE_FROM_TEMPORARY_STRING
)

add_should_fail_test(
    Message.cpp
    EVENT_ATTRIBUTES_FROM_MESSAGE_FROM_TEMPORARY_WSTRING
)

add_should_fail_test(
    Message.cpp
    START_RANGE_IN_FROM_MESSAGE_FROM_TEMPORARY_WSTRING
)
