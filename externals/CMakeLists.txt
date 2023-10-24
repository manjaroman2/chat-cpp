target_include_directories(${PROJECT_NAME}
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/async-sockets-cpp/async-sockets/include
)

target_include_directories(${CHAT_GUI}
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/async-sockets-cpp/async-sockets/include 
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/imtui/include/imtui
)

add_subdirectory(magic_enum)