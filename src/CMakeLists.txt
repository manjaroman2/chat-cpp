
if (ETA_TUI)
    add_executable(${ETA_TUI} eta.cpp eta.hpp api.hpp)
    target_link_libraries(${ETA_TUI} 
        magic_enum 
        # imtui
        imtui-ncurses
    )
endif()

if (DELTA_SERVER)
    add_executable(${DELTA_SERVER} delta.cpp delta.hpp api.hpp)
    target_link_libraries(${DELTA_SERVER}
        pthread
        magic_enum
        # async-sockets
    )
    target_include_directories(${DELTA_SERVER} PUBLIC ../externals/async-sockets-cpp/async-sockets)
    # echo(${CMAKE_CURRENT_LIST_DIR})
    # target_include_directories(${DELTA_SERVER}
    #     PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../externals/async-sockets-cpp/async-sockets/include 
    # )
endif()

