#include <thcrap.h>
#include <lua/include/lua.hpp>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx9.h>
#include <imgui/backends/imgui_impl_win32.h>

struct thconsole
{
    char                  InputBuf[256];
    ImVector<char*>       Items;
    ImVector<char*>       History;
    int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;

    thconsole();
    ~thconsole();
    void    ClearLog();
    void    AddLog(const char* fmt, ...);
    void AddRawLog(const char* str);
    void    Draw(const char* title);
    void    ExecCommand(const char* command_line);
    int     TextEditCallback(ImGuiInputTextCallbackData* data);
};

const char* exec_command(const char* cmd);
