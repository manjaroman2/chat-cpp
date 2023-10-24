add_executable(${PROJECT_NAME} main.cpp main.hpp api.hpp)
# add_executable(${PROJECT_NAME} main2.cpp gui.hpp)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE pthread
    PRIVATE magic_enum
    PRIVATE ncurses
    PRIVATE panel
    PRIVATE fmt
)
