#include "builtins.h"
#include "../object.h"
#include "../value.h"

#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#endif

SapphireValue native_io_write_file(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) {
        return false;
    }
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string content = static_cast<ObjString*>(args[1].as.obj)->chars;

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    file.close();
    return true;
}

SapphireValue native_io_read_file(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;

    std::ifstream file(path);
    if (!file.is_open()) return {};

    std::stringstream buffer;
    buffer << file.rdbuf();
    return new_string(g_current_vm, buffer.str());
}

SapphireValue native_io_open_file_dialog(int arg_count, SapphireValue* args) {
#ifdef _WIN32
    char filename[MAX_PATH];
    filename[0] = '\0';

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "All Files\0*.*\0Sapphire Scripts (*.sp)\0*.sp\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "sp";

    if (GetOpenFileNameA(&ofn)) {
        return new_string(g_current_vm, std::string(filename));
    }
#elif defined(__APPLE__)
    // macOS: Use zenity or similar (requires user to install)
    // For now, return empty string as native file dialog is complex
    return new_string(g_current_vm, "");
#elif defined(__linux__)
    // Linux: Use zenity if available
    FILE* pipe = popen("zenity --file-selection --file-filter='Sapphire Scripts | *.sp' --file-filter='All Files | *.*' 2>/dev/null", "r");
    if (pipe) {
        char buffer[PATH_MAX];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = '\0';
            pclose(pipe);
            return new_string(g_current_vm, std::string(buffer));
        }
        pclose(pipe);
    }
#endif
    return new_string(g_current_vm, "");
}

SapphireValue native_io_file_size(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_STRING) return -1.0;
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    try {
        if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
            return (double)std::filesystem::file_size(path);
        }
    } catch (...) {
        return -1.0;
    }
    return -1.0;
}

SapphireValue native_io_is_dir(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_STRING) return false;
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    try {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    } catch (...) {
        return false;
    }
}

SapphireValue native_io_exists(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::ifstream file(path);
    return file.good();
}

SapphireValue native_io_print_color(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[1], OBJ_STRING)) return {};
    std::string color = static_cast<ObjString*>(args[1].as.obj)->chars;

    std::string code = "\033[0m";
    if (color == "red") code = "\033[31m";
    else if (color == "green") code = "\033[32m";
    else if (color == "yellow") code = "\033[33m";
    else if (color == "blue") code = "\033[34m";
    else if (color == "cyan") code = "\033[36m";
    else if (color == "magenta") code = "\033[35m";
    else if (color == "white") code = "\033[37m";
    else if (color == "black") code = "\033[30m";
    else if (color.size() == 7 && color[0] == '#') {
        try {
            int r = std::stoi(color.substr(1, 2), nullptr, 16);
            int g = std::stoi(color.substr(3, 2), nullptr, 16);
            int b = std::stoi(color.substr(5, 2), nullptr, 16);
            code = "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
        } catch(...) {}
    } else if (color.substr(0, 4) == "rgb(" && color.back() == ')') {
        try {
            std::string inner = color.substr(4, color.size() - 5);
            size_t p1 = inner.find(',');
            size_t p2 = inner.find(',', p1 + 1);
            if (p1 != std::string::npos && p2 != std::string::npos) {
                int r = std::stoi(inner.substr(0, p1));
                int g = std::stoi(inner.substr(p1 + 1, p2 - p1 - 1));
                int b = std::stoi(inner.substr(p2 + 1));
                code = "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
            }
        } catch(...) {}
    }

    std::cout << code;
    print_value(args[0]);
    std::cout << "\033[0m"; // Sem std::endl
    return {};
}

SapphireValue native_io_read_input(int arg_count, SapphireValue* args) {
    std::string result = "";
#ifdef _WIN32
    // Check if there are console events
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD numEvents = 0;
    if (GetNumberOfConsoleInputEvents(hInput, &numEvents) && numEvents > 0) {
        while (_kbhit()) {
            int ch = _getch();
            result += (char)ch;
        }
    }
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        char buf[256];
        ssize_t bytes = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (bytes > 0) {
            buf[bytes] = '\0';
            result = buf;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
    return new_string(g_current_vm, result);
}

SapphireValue native_io_delete_file(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    return std::remove(path.c_str()) == 0;
}

SapphireValue native_io_append_file(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) {
        return false;
    }
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string content = static_cast<ObjString*>(args[1].as.obj)->chars;

    std::ofstream file(path, std::ios_base::app);
    if (!file.is_open()) return false;
    file << content;
    file.close();
    return true;
}

