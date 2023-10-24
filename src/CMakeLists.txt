add_executable(${CHAT_SERVER} main.cpp main.hpp api.hpp)
target_link_libraries(${CHAT_SERVER} 
    PRIVATE pthread
    PRIVATE magic_enum
    PRIVATE ncurses
    PRIVATE panel
)

add_executable(${CHAT_GUI} gui.cpp gui.hpp api.hpp)
target_link_libraries(${CHAT_GUI} 
    PRIVATE magic_enum
    PRIVATE imtui)