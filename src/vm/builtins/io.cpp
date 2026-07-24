// io.cpp — Sapphire IO Builtins (v1.1.0)
// New in v1.1.0:
//   IO.listDir(path)          -> array of filenames
//   IO.listDirRecursive(path) -> array of full paths
//   IO.copyFile(src, dst)     -> bool
//   IO.moveFile(src, dst)     -> bool
//   IO.rename(old, new)       -> bool (alias for moveFile)
//   IO.makeDir(path)          -> bool
//   IO.makeAllDirs(path)      -> bool
//   IO.getTempDir()           -> string
//   IO.readLines(path)        -> array of strings
//   IO.readBinary(path)       -> string (raw bytes)
//   IO.writeBinary(path, data)-> bool
//   IO.getAbsolutePath(path)  -> string
//   IO.getParentDir(path)     -> string
//   IO.getExtension(path)     -> string
//   IO.getBasename(path)      -> string
//   IO.isFile(path)           -> bool
// Fixed in v1.1.0:
//   IO.exists() now uses std::filesystem::exists() instead of opening ifstream
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

namespace fs = std::filesystem;

// Helper: convert a std::string arg safely
static std::string get_string_arg(SapphireValue* args, int idx) {
    return static_cast<ObjString*>(args[idx].as.obj)->chars;
}

// ─────────────────────────────────────────────
// Existing functions (fixed where noted)
// ─────────────────────────────────────────────

SapphireValue native_io_write_file(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING))
        return false;
    std::string path    = get_string_arg(args, 0);
    std::string content = get_string_arg(args, 1);

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    file.close();
    return true;
}

SapphireValue native_io_read_file(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = get_string_arg(args, 0);

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
    return new_string(g_current_vm, "");
#elif defined(__linux__)
    FILE* pipe = popen("zenity --file-selection --file-filter='Sapphire Scripts | *.sp' --file-filter='All Files | *.*' 2>/dev/null", "r");
    if (pipe) {
        char buffer[PATH_MAX];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
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
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return -1.0;
    std::string path = get_string_arg(args, 0);
    try {
        if (fs::exists(path) && fs::is_regular_file(path)) {
            return static_cast<double>(fs::file_size(path));
        }
    } catch (...) {}
    return -1.0;
}

SapphireValue native_io_is_dir(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = get_string_arg(args, 0);
    try {
        return fs::exists(path) && fs::is_directory(path);
    } catch (...) { return false; }
}

// FIX v1.1.0: use fs::exists instead of opening ifstream (no side effects)
SapphireValue native_io_exists(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = get_string_arg(args, 0);
    try {
        return fs::exists(path);
    } catch (...) { return false; }
}

SapphireValue native_io_print_color(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[1], OBJ_STRING)) return {};
    std::string color = get_string_arg(args, 1);

    std::string code = "\033[0m";
    if (color == "red")          code = "\033[31m";
    else if (color == "green")   code = "\033[32m";
    else if (color == "yellow")  code = "\033[33m";
    else if (color == "blue")    code = "\033[34m";
    else if (color == "cyan")    code = "\033[36m";
    else if (color == "magenta") code = "\033[35m";
    else if (color == "white")   code = "\033[37m";
    else if (color == "black")   code = "\033[30m";
    else if (color.size() == 7 && color[0] == '#') {
        try {
            int r = std::stoi(color.substr(1, 2), nullptr, 16);
            int g = std::stoi(color.substr(3, 2), nullptr, 16);
            int b = std::stoi(color.substr(5, 2), nullptr, 16);
            code = "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
        } catch (...) {}
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
        } catch (...) {}
    }

    std::cout << code;
    print_value(args[0]);
    std::cout << "\033[0m";
    return {};
}

SapphireValue native_io_read_input(int arg_count, SapphireValue* args) {
    std::string result = "";
#ifdef _WIN32
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD numEvents = 0;
    if (GetNumberOfConsoleInputEvents(hInput, &numEvents) && numEvents > 0) {
        while (_kbhit()) {
            int ch = _getch();
            result += static_cast<char>(ch);
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
    std::string path = get_string_arg(args, 0);
    try {
        return fs::remove(fs::path(path));
    } catch (...) { return false; }
}

SapphireValue native_io_append_file(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING))
        return false;
    std::string path    = get_string_arg(args, 0);
    std::string content = get_string_arg(args, 1);

    std::ofstream file(path, std::ios_base::app);
    if (!file.is_open()) return false;
    file << content;
    file.close();
    return true;
}

// ─────────────────────────────────────────────
// NEW in v1.1.0 — Advanced filesystem functions
// ─────────────────────────────────────────────

SapphireValue native_io_list_dir(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return new_array(g_current_vm);
    std::string path = get_string_arg(args, 0);

    ObjArray* arr = new_array(g_current_vm);
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            arr->elements.push_back(SapphireValue(new_string(g_current_vm, name)));
        }
    } catch (...) {}
    return arr;
}

SapphireValue native_io_list_dir_recursive(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return new_array(g_current_vm);
    std::string path = get_string_arg(args, 0);

    ObjArray* arr = new_array(g_current_vm);
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            std::string full = entry.path().string();
            arr->elements.push_back(SapphireValue(new_string(g_current_vm, full)));
        }
    } catch (...) {}
    return arr;
}

SapphireValue native_io_copy_file(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING))
        return false;
    std::string src = get_string_arg(args, 0);
    std::string dst = get_string_arg(args, 1);
    try {
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) { return false; }
}

SapphireValue native_io_move_file(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING))
        return false;
    std::string src = get_string_arg(args, 0);
    std::string dst = get_string_arg(args, 1);
    try {
        fs::rename(src, dst);
        return true;
    } catch (...) { return false; }
}

SapphireValue native_io_rename(int arg_count, SapphireValue* args) {
    // Alias for IO.moveFile
    return native_io_move_file(arg_count, args);
}

SapphireValue native_io_make_dir(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = get_string_arg(args, 0);
    try {
        return fs::create_directory(path);
    } catch (...) { return false; }
}

SapphireValue native_io_make_all_dirs(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = get_string_arg(args, 0);
    try {
        return fs::create_directories(path);
    } catch (...) { return false; }
}

SapphireValue native_io_get_temp_dir(int arg_count, SapphireValue* args) {
    try {
        return new_string(g_current_vm, fs::temp_directory_path().string());
    } catch (...) {
        return new_string(g_current_vm, "/tmp");
    }
}

SapphireValue native_io_read_lines(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return new_array(g_current_vm);
    std::string path = get_string_arg(args, 0);

    ObjArray* arr = new_array(g_current_vm);
    std::ifstream file(path);
    if (!file.is_open()) return arr;

    std::string line;
    while (std::getline(file, line)) {
        arr->elements.push_back(SapphireValue(new_string(g_current_vm, line)));
    }
    return arr;
}

SapphireValue native_io_read_binary(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = get_string_arg(args, 0);

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return {};

    std::ostringstream buf;
    buf << file.rdbuf();
    return new_string(g_current_vm, buf.str());
}

SapphireValue native_io_write_binary(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING))
        return false;
    std::string path = get_string_arg(args, 0);
    std::string data = get_string_arg(args, 1);

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    return true;
}

SapphireValue native_io_get_absolute_path(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = get_string_arg(args, 0);
    try {
        return new_string(g_current_vm, fs::absolute(path).string());
    } catch (...) { return {}; }
}

SapphireValue native_io_get_parent_dir(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = get_string_arg(args, 0);
    return new_string(g_current_vm, fs::path(path).parent_path().string());
}

SapphireValue native_io_get_extension(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = get_string_arg(args, 0);
    return new_string(g_current_vm, fs::path(path).extension().string());
}

SapphireValue native_io_get_basename(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = get_string_arg(args, 0);
    return new_string(g_current_vm, fs::path(path).filename().string());
}

SapphireValue native_io_is_file(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = get_string_arg(args, 0);
    try {
        return fs::is_regular_file(path);
    } catch (...) { return false; }
}
