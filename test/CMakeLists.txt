include(${Textum_SOURCE_DIR}/cmake/doctest.cmake)

add_executable(textum-unit-tests test_main.cpp)
target_sources(textum-unit-tests PRIVATE
    textum/aho_corasick.cpp
    textum/fsm.cpp
    textum/trie.cpp
)
target_link_libraries(textum-unit-tests
    PRIVATE
        Textum::textum
        doctest::doctest
)

find_program(GCOVR_EXECUTABLE gcovr)
if(TEXTUM_COVERAGE AND GCOVR_EXECUTABLE)
    message(STATUS "Измерение покрытия кода тестами включено")

    target_compile_options(textum-unit-tests PRIVATE --coverage)
    target_link_libraries(textum-unit-tests PRIVATE gcov)

    add_custom_target(coverage
        COMMAND
            ${GCOVR_EXECUTABLE}
                --root=${PROJECT_SOURCE_DIR}/
                --filter=${PROJECT_SOURCE_DIR}/include
                --filter=${PROJECT_SOURCE_DIR}/src
                --object-directory=${PROJECT_BINARY_DIR}
        DEPENDS
            check
    )
elseif(TEXTUM_COVERAGE AND NOT GCOVR_EXECUTABLE)
    set(TEXTUM_COVERAGE OFF)
    message(WARNING "Для замеров покрытия кода тестами требуется программа gcovr")
endif()

add_custom_target(check ALL COMMAND textum-unit-tests)
