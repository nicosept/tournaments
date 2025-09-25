project(tournament_tests)


set(TEST_SOURCES
        controller/TeamControllerTest.cpp
        controller/TournamentControllerTest.cpp
)
set(SOURCES ${SERVICES_SOURCES} ${TEST_SOURCES})

find_package(GTest CONFIG REQUIRED)

include_directories(../include)

add_executable(${PROJECT_NAME}_runner
    ${TEST_SOURCES}
)

target_link_libraries(${PROJECT_NAME}_runner PRIVATE
        tournament_services_lib
        tournament_common
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main)

# add_test(AllTestsInMain ${PROJECT_NAME}_runner)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_runner)