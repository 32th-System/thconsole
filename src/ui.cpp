#include "thconsole.h"
#include <d3d9.h>

LPDIRECT3DDEVICE9 d3ddevice = NULL;
LPDIRECT3DTEXTURE9 texture = NULL;
HWND hwnd = NULL;

static bool initialized = false;
extern lua_State* lua_state;

typedef HRESULT __stdcall IDirect3DDevice9_Present_t(IDirect3DDevice9*, RECT*, RECT*, HWND, RGNDATA*);
IDirect3DDevice9_Present_t* orig_Present = NULL;

bool console_open = false;

static int   Im_Stricmp(const char* s1, const char* s2) { int d; while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; } return d; }
static int   Im_Strnicmp(const char* s1, const char* s2, int n) { int d = 0; while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; n--; } return d; }
static char* Im_Strdup(const char* s) { IM_ASSERT(s); size_t len = strlen(s) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
static void  Im_Strtrim(char* s) { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

// Modified version of the console example in imgui_demo.cpp
thconsole::thconsole()
{
    ClearLog();
    memset(InputBuf, 0, sizeof(InputBuf));
    HistoryPos = -1;
    AutoScroll = true;
    ScrollToBottom = false;
    AddLog("Welcome to thconsole!");
}
thconsole::~thconsole()
{
    ClearLog();
    for (int i = 0; i < History.Size; i++)
        free(History[i]);
}

void thconsole::ClearLog()
{
    for (int i = 0; i < Items.Size; i++)
        free(Items[i]);
    Items.clear();
}

void thconsole::AddLog(const char* fmt, ...)
{
    // FIXME-OPT

    va_list args;
    va_start(args, fmt);
    // random slot number I generated with Google
    const char* buf = strings_vsprintf(4446054, fmt, args);
    va_end(args);
    this->AddRawLog(buf);
}

void thconsole::AddRawLog(const char* str) {
    this->Items.push_back(Im_Strdup(str));
}

void thconsole::Draw(const char* title)
{
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin(title);

    // As a specific feature guaranteed by the library, after calling Begin() the last Item represent the title bar.
    // So e.g. IsItemHovered() will return true when hovering the title bar.
    // Here we create a context menu only available from the title bar.

    if (ImGui::SmallButton("Clear"))
        ClearLog();
    ImGui::SameLine();
    if (ImGui::SmallButton("Close"))
        console_open = false;
    ImGui::SameLine();
    bool copy_to_clipboard = ImGui::SmallButton("Copy");
    //static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }

    ImGui::Separator();
    ImGui::Checkbox("Auto-scroll", &AutoScroll);

    ImGui::SameLine();
    Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
    ImGui::Separator();

    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Display every line as a separate entry so we can change their color or add custom widgets.
    // If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
    // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
    // to only process visible items. The clipper will automatically measure the height of your first item and then
    // "seek" to display only items in the visible area.
    // To use the clipper we can replace your standard loop:
    //      for (int i = 0; i < Items.Size; i++)
    //   With:
    //      ImGuiListClipper clipper;
    //      clipper.Begin(Items.Size);
    //      while (clipper.Step())
    //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
    // - That your items are evenly spaced (same height)
    // - That you have cheap random access to your elements (you can access them given their index,
    //   without processing all the ones before)
    // You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
    // We would need random-access on the post-filtered list.
    // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
    // or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
    // and appending newly elements as they are inserted. This is left as a task to the user until we can manage
    // to improve this example code!
    // If your items are of variable height:
    // - Split them into same height items would be simpler and facilitate random-seeking into your list.
    // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    if (copy_to_clipboard)
        ImGui::LogToClipboard();
    for (int i = 0; i < Items.Size; i++)
    {
        const char* item = Items[i];
        if (!Filter.PassFilter(item))
            continue;

        // Normally you would store more information in your item than just a string.
        // (e.g. make Items[] an array of structure, store color/type etc.)
        ImVec4 color;
        bool has_color = false;
        if (strstr(item, "[error]")) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
        else if (strncmp(item, "# ", 2) == 0) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
        if (has_color)
            ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(item);
        if (has_color)
            ImGui::PopStyleColor();
    }
    if (copy_to_clipboard)
        ImGui::LogFinish();

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    ScrollToBottom = false;

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();

    // Command-line
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
    if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, [](ImGuiInputTextCallbackData* data) -> int {
        thconsole* console = (thconsole*)data->UserData;
        return console->TextEditCallback(data);
    }, (void*)this))

    {
        char* s = InputBuf;
        Im_Strtrim(s);
        if (s[0])
            ExecCommand(s);
        strcpy(s, "");
        reclaim_focus = true;
    }

    // Auto-focus on window apparition
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

    ImGui::End();
}

void thconsole::ExecCommand(const char* command_line)
{
    AddLog("# %s\n", command_line);

    // Insert into history. First find match and delete it so it can be pushed to the back.
    // This isn't trying to be smart or optimal.
    HistoryPos = -1;
    for (int i = History.Size - 1; i >= 0; i--)
        if (Im_Stricmp(History[i], command_line) == 0)
        {
            free(History[i]);
            History.erase(History.begin() + i);
            break;
        }
    History.push_back(Im_Strdup(command_line));
    // On command input, we scroll to bottom even if AutoScroll==false

    const char* out = exec_command(command_line);
    if (out) {
        this->AddRawLog(out);
        free((void*)out);
    }

    ScrollToBottom = true;
}

int thconsole::TextEditCallback(ImGuiInputTextCallbackData* data)
{
    //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackHistory:
        // Example of HISTORY
        const int prev_history_pos = HistoryPos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (HistoryPos == -1)
                HistoryPos = History.Size - 1;
            else if (HistoryPos > 0)
                HistoryPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (HistoryPos != -1)
                if (++HistoryPos >= History.Size)
                    HistoryPos = -1;
        }

        // A better implementation would preserve the data on the current input line along with cursor position.
        if (prev_history_pos != HistoryPos)
        {
            const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, history_str);
        }
    }
    return 0;
}

thconsole console;

HRESULT __stdcall hook_Present(IDirect3DDevice9* that, RECT* src, RECT* dst, HWND hDestWindowOverride, RGNDATA* dirty) {
	if (!ImGui::GetCurrentContext() || !console_open) goto end;

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

    console.Draw("thconsole");

	ImGui::EndFrame();

	d3ddevice->BeginScene();
	ImGui::Render();

	DWORD orig;
	d3ddevice->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &orig);
	d3ddevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
	d3ddevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, orig);

	d3ddevice->EndScene();
end:
	return orig_Present(that, src, dst, hDestWindowOverride, dirty);
}

static WNDPROC orig_wndProc = NULL;
typedef ATOM WINAPI RegisterClassA_type(const WNDCLASSA* lpWndClass);
DETOUR_CHAIN_DEF(RegisterClassA);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK ui_wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;
	else
		return orig_wndProc(hWnd, msg, wParam, lParam);
}

ATOM WINAPI hook_RegisterClassA(WNDCLASSA* lpWndClass) {
	orig_wndProc = lpWndClass->lpfnWndProc;
	lpWndClass->lpfnWndProc = ui_wndProc;
	return chain_RegisterClassA(lpWndClass);
}

int BP_thconsole_lock_input(x86_reg_t* regs, json_t* bp_info) {
    if (console_open)
        return 0;
    else
        return 1;
}

int BP_thconsole_toggle(x86_reg_t* regs, json_t* bp_info) {
    if (json_object_get_immediate(bp_info, regs, "toggle")) {
        console_open = true;
    }
    return breakpoint_cave_exec_flag(bp_info);
}


int BP_thconsole_ui_init(x86_reg_t* regs, json_t* bp_info) {
	ImGui::CreateContext();

	hwnd = (HWND)json_object_get_immediate(bp_info, regs, "hwnd");
	d3ddevice = (LPDIRECT3DDEVICE9)json_object_get_immediate(bp_info, regs, "d3ddevice");
	vtable_detour_t det[] = {
		{17, (void*)hook_Present, (void**)&orig_Present}
	};

	vtable_detour(*(void***)d3ddevice, det, elementsof(det));

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(d3ddevice);

	ImGuiIO& io = ImGui::GetIO();

	io.DeltaTime = 1.0f / 60.0f;
	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 16.0f);

	return breakpoint_cave_exec_flag(bp_info);
}

void thconsole_ui_mod_detour(void) {
	detour_chain("user32.dll", 1,
		"RegisterClassA", hook_RegisterClassA, &chain_RegisterClassA,
	NULL);	
}
