add_definitions(-DDEBUG)
add_executable(server demo/multi_server.cpp ${SOURCES})
target_link_libraries(server pthread)

