add_executable(ActorPoolTests
    ActorPoolTests.cpp
)

target_include_directories(ActorPoolTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Framework/src
)

target_link_libraries(ActorPoolTests PRIVATE 101Framework)

add_test(NAME ActorPoolTests COMMAND ActorPoolTests)
set_tests_properties(ActorPoolTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
