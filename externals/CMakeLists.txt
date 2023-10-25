# find_library(IMTUI imtui)
# if (NOT IMTUI) 
# endif()
# link_directories(imtui)
# include_directories(imtui)
add_subdirectory(imtui)


# target_include_directories(${CHAT_GUI}
#     PUBLIC ${CMAKE_CURRENT_LIST_DIR}/async-sockets-cpp/async-sockets/include 
#     )
add_subdirectory(magic_enum)

