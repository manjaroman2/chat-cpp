target_include_directories(${PROJECT_NAME}
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/async-sockets-cpp/async-sockets/include
)
add_subdirectory(magic_enum)
add_subdirectory(fmt)