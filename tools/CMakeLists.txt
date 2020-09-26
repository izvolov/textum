add_executable(search search.cpp)
target_link_libraries(search Textum::textum)

add_executable(suggest suggest.cpp)
target_link_libraries(suggest Textum::textum)
