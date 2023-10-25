#include <imtui/imtui.h>
#include <imtui/imtui-impl-ncurses.h>
#include <iostream>
#include <pthread.h>
#include "eta.hpp"
#include "api.hpp"

namespace Eta
{
    ImTui::TScreen *screen;
    void loop()
    {
        bool demo = true;
        int nframes = 0;
        float fval = 1.23f;

        while (true)
        {
            ImTui_ImplNcurses_NewFrame();
            ImTui_ImplText_NewFrame();

            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(4, 27), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(50.0, 10.0), ImGuiCond_Once);
            ImGui::Begin("Hello, world!");
            ImGui::Text("NFrames = %d", nframes++);
            ImGui::Text("Mouse Pos : x = %g, y = %g", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
            ImGui::Text("Time per frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text("Float:");
            ImGui::SameLine();
            ImGui::SliderFloat("##float", &fval, 0.0f, 10.0f);

#ifndef __EMSCRIPTEN__
            ImGui::Text("%s", "");
            if (ImGui::Button("Exit program", {ImGui::GetContentRegionAvail().x, 2}))
            {
                break;
            }
#endif

            ImGui::End();

            // ImTui::ShowDemoWindow(&demo);

            ImGui::Render();

            ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
            ImTui_ImplNcurses_DrawScreen();
        }
    }

    void boot()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        screen = ImTui_ImplNcurses_Init(true);
        ImTui_ImplText_Init();
    }

    void shutdown()
    {
        ImTui_ImplText_Shutdown();
        ImTui_ImplNcurses_Shutdown();
    }

    int main()
    {
        boot();
        loop();
        shutdown();
        return 0;
    }

}

int main()
{
    return Eta::main();
}
// #include "api.hpp"
// #include <iostream>
// #include <stdio.h>

// int main(int argc, char **argv)
// {
//     MagicType magic, connId;
//     MessageLengthType messageLength;
//     char magicBuffer[Api::MAGIC_TYPE_SIZE];
//     char messageLengthBuffer[Api::MESSAGE_LENGTH_TYPE_SIZE];
//     char messageBuffer[Api::MAX_MESSAGE_LENGTH];
//     Api::buffer_read_all(STDIN_FILENO, magicBuffer, Api::MAGIC_TYPE_SIZE);
//     Api::buffer_read_all(STDIN_FILENO, messageLengthBuffer, Api::MESSAGE_LENGTH_TYPE_SIZE);
//     // Convert 2 Bytes to ushort
//     memcpy(&messageLength, &messageLengthBuffer, Api::MESSAGE_LENGTH_TYPE_SIZE);
//     Api::buffer_read_all(STDIN_FILENO, messageBuffer, messageLength);
//     memcpy(&magic, magicBuffer, Api::MAGIC_TYPE_SIZE);
//     switch (magic)
//     {
//     case Api::LOG_INFO:
//         std::cout << "LOG_INFO: " << messageBuffer << std::endl;
//         break;
//     }
//     return 0;
// }