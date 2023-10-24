#include "gui.hpp"
#include <thread>

int main(int argc, char **argv)
{
    funny::Gui gui = funny::Gui();
    std::thread t1(&funny::Gui::loop, &gui);
    // while (!gui.is_stopped())
    // {
    //     if (gui.is_resize())
    //     {
    //         gui.on_term_resize();
    //     }
    // }
    t1.join();
    return 0;
}