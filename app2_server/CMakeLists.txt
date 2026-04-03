add_executable(app2_server main.cpp logger.cpp)

target_link_libraries(app2_server PRIVATE
    pthread
)
