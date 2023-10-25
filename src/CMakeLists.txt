# add_executable(${CHAT_SERVER} main.cpp main.hpp api.hpp)
# target_link_libraries(${CHAT_SERVER} 
#     PRIVATE pthread
#     PRIVATE magic_enum
# )


add_executable(${CHAT_GUI} gui.cpp gui.hpp api.hpp)
target_link_libraries(${CHAT_GUI} 
    imtui
    imtui-ncurses
)
# target_include_directories(${CHAT_SERVER}
#     PUBLIC ${CMAKE_CURRENT_LIST_DIR}/async-sockets-cpp/async-sockets/include
# )
# target_link_libraries(${CHAT_GUI} 
#     magic_enum
#     imtui
#     ncurses
# )