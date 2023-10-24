find_library(IMTUI imtui)
if (NOT IMTUI) 
    add_subdirectory(imtui)
endif()
find_library(ASYNC_SOCKETS async-sockets-cpp)
if (NOT ASNYC_SOCKETS)
    target_include_directories(${CHAT_SERVER}
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/async-sockets-cpp/async-sockets/include
    )

    target_include_directories(${CHAT_GUI}
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/async-sockets-cpp/async-sockets/include 
    )
endif()

find_library(MAGIC_ENUM magic_enum)
if (NOT MAGIC_ENUM)
    add_subdirectory(magic_enum)
endif()

