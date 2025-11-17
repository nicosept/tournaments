project(tournament_consumer)

set(CMAKE_CXX_STANDARD 23)
set(CONSUMER_SOURCES
        src/delegate/BracketGenerator.cpp
)

include(CTest)
enable_testing()

find_package(libpqxx CONFIG REQUIRED)
find_path(HYPODERMIC_INCLUDE_DIRS "Hypodermic/ActivatedRegistrationInfo.h")
find_package(nlohmann_json CONFIG REQUIRED)

add_subdirectory(tests)

include_directories(include)

add_executable(${PROJECT_NAME}
        main.cpp
        ${CONSUMER_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Crow::Crow
        asio::asio
        nlohmann_json::nlohmann_json
        libpqxx::pqxx
        unofficial::activemq-cpp::activemq-cpp
        tournament_common
)

target_include_directories(${PROJECT_NAME} PRIVATE ${HYPODERMIC_INCLUDE_DIRS})

configure_file(
        ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/configuration.json   # source file
        ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/configuration.json  # destination
        COPYONLY
)