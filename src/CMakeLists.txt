
add_executable(${PROJECT_NAME} main.cpp main.hpp api.hpp)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE pthread
    PRIVATE magic_enum
    PRIVATE ncurses
    PRIVATE panel
)

add_executable(${CHAT_GUI} gui/gui.cpp gui/gui.hpp api.hpp)
target_link_libraries(${CHAT_GUI} 
    PRIVATE magic_enum)