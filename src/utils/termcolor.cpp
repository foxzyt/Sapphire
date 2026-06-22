#include "termcolor.h"

#ifdef _WIN32
#include <windows.h>
#endif

static bool g_colors_enabled = false;

void init_terminal() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    
    // Tentativa de habilitar as cores ANSI (Virtual Terminal Processing)
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, dwMode) != 0) {
        g_colors_enabled = true;
    }
#else
    // No Linux/Mac as cores quase sempre funcionam por padrão
    g_colors_enabled = true;
#endif
}

std::string tc_reset()  { return g_colors_enabled ? "\x1b[0m" : ""; }
std::string tc_red()    { return g_colors_enabled ? "\x1b[31m" : ""; }
std::string tc_green()  { return g_colors_enabled ? "\x1b[32m" : ""; }
std::string tc_yellow() { return g_colors_enabled ? "\x1b[33m" : ""; }
std::string tc_blue()   { return g_colors_enabled ? "\x1b[34m" : ""; }
std::string tc_cyan()   { return g_colors_enabled ? "\x1b[36m" : ""; }
std::string tc_bold()   { return g_colors_enabled ? "\x1b[1m" : ""; }
