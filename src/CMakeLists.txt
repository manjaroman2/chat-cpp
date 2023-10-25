
if (CHAT_GUI)
    add_executable(${CHAT_GUI} gui.cpp gui.hpp api.hpp)
    target_link_libraries(${CHAT_GUI} 
        magic_enum 
        # imtui
        imtui-ncurses
    )
endif()

if (CHAT_SERVER)
    add_executable(${CHAT_SERVER} main.cpp main.hpp api.hpp)
    target_link_libraries(${CHAT_SERVER}
        pthread
        magic_enum
        # async-sockets
    )
    target_include_directories(${CHAT_SERVER} PUBLIC ../externals/async-sockets-cpp/async-sockets)
    # echo(${CMAKE_CURRENT_LIST_DIR})
    # target_include_directories(${CHAT_SERVER}
    #     PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../externals/async-sockets-cpp/async-sockets/include 
    # )
endif()

