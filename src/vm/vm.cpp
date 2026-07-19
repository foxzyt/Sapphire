// Corundum VM Implementation
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <sstream>
#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "debug.h"
#include "value.h"
#include "config.h"
#include "termcolor.h"
#include "sapphire_api.h"
#include "opencl_api.h"
#include "sqlite_api.h"
#include "utils.h"
#include "opcodes.h"
#include "httplib.h"
#include "tokens.h"
#include "nlohmann/json.hpp"
#include "opencl_api.h"
#include "preprocessor/preprocessor.h"
#include "bytecode_io.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <vector>
#include <cmath>
#include <variant>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <set>
#include <random>
#include "engine.h"
#include "vec2d.h"
#include "vec3d.h"
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <conio.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#endif

static std::random_device rd;
static std::mt19937 gen(rd());

thread_local VM* g_current_vm = nullptr;

static std::mutex thread_mutex;
static std::map<int, std::thread> active_threads;
static int next_thread_id = 1;

static SapphireValue native_spawn(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return SapphireValue();
    std::string script_path = static_cast<ObjString*>(args[0].as.obj)->chars;
    
    int tid;
    {
        std::lock_guard<std::mutex> lock(thread_mutex);
        tid = next_thread_id++;
        active_threads[tid] = std::thread([script_path]() {
            try {
                std::string src = load_file_as_string(script_path);
                ScriptConfig config;
                VM thread_vm(config, false, nullptr);
                thread_vm.interpret(src);
            } catch (...) {}
        });
    }
    return SapphireValue((double)tid);
}

static std::mutex global_mutexes_lock;
static std::map<int, std::shared_ptr<std::mutex>> global_mutexes;
static int next_mutex_id = 1;

static SapphireValue native_mutex_new(int arg_count, SapphireValue* args) {
    std::lock_guard<std::mutex> lock(global_mutexes_lock);
    int id = next_mutex_id++;
    global_mutexes[id] = std::make_shared<std::mutex>();
    return SapphireValue((double)id);
}

static SapphireValue native_mutex_lock(int arg_count, SapphireValue* args) {
    if(arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return SapphireValue(false);
    int id = (int)args[0].as.number;
    std::shared_ptr<std::mutex> m;
    {
        std::lock_guard<std::mutex> lock(global_mutexes_lock);
        if(!global_mutexes.count(id)) return SapphireValue(false);
        m = global_mutexes[id];
    }
    m->lock();
    return SapphireValue(true);
}

static SapphireValue native_mutex_unlock(int arg_count, SapphireValue* args) {
    if(arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return SapphireValue(false);
    int id = (int)args[0].as.number;
    std::shared_ptr<std::mutex> m;
    {
        std::lock_guard<std::mutex> lock(global_mutexes_lock);
        if(!global_mutexes.count(id)) return SapphireValue(false);
        m = global_mutexes[id];
    }
    m->unlock();
    return SapphireValue(true);
}

static SapphireValue native_join(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return SapphireValue(false);
    int tid = (int)args[0].as.number;
    
    std::thread t;
    {
        std::lock_guard<std::mutex> lock(thread_mutex);
        if (active_threads.count(tid)) {
            t = std::move(active_threads[tid]);
            active_threads.erase(tid);
        }
    }
    
    if (t.joinable()) {
        t.join();
        return SapphireValue(true);
    }
    return SapphireValue(false);
}

static SapphireValue native_system_core_count(int arg_count, SapphireValue* args) {
    unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0) cores = 4;
    return SapphireValue((double)cores);
}

static SapphireValue convertJsonToSapphire(VM* vm, const nlohmann::json& j);

static SapphireValue convertJsonObjectToSapphireMap(VM* vm, const nlohmann::json& j) {
    ObjMap* map_obj = new_map(vm);
    vm->push(SapphireValue(map_obj));
    for (auto it = j.begin(); it != j.end(); ++it) {
        std::string key_copy = it.key();
        const nlohmann::json& value = it.value();
        map_obj->items[key_copy] = convertJsonToSapphire(vm, value);
    }
    vm->pop();
    return map_obj;
}

static SapphireValue convertJsonArrayToSapphireArray(VM* vm, const nlohmann::json& j) {
    auto array_obj = new_array(g_current_vm);
    for (const auto& element : j) {
        array_obj->elements.push_back(convertJsonToSapphire(vm, element));
    }
    return array_obj;
}

static SapphireValue convertJsonToSapphire(VM* vm, const nlohmann::json& j) {
    if (j.is_object()) return convertJsonObjectToSapphireMap(vm, j);
    if (j.is_array()) return convertJsonArrayToSapphireArray(vm, j);
    if (j.is_string()) return new_string(vm, j.get<std::string>());
    if (j.is_number()) return j.get<double>();
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_null()) return {};
    return {};
}


static const auto clock_start_time = std::chrono::high_resolution_clock::now();
static SapphireValue clock_native(int arg_count, SapphireValue* args) {
    if (arg_count != 0) return {};
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = now - clock_start_time;
    return diff.count();
}

static SapphireValue assert_native(int arg_count, SapphireValue* args) {
    if (arg_count < 1) {
        throw std::runtime_error("assert() expects at least 1 argument.");
    }
    bool condition = !is_falsey(args[0]);
    if (!condition) {
        std::string message = "Assertion failed.";
        if (arg_count >= 2 && args[1].type == ValType::VAL_OBJ) {
            Obj* obj = args[1].as.obj;
            if (obj->type == OBJ_STRING) {
                message = static_cast<ObjString*>(obj)->chars;
            }
        }
        throw std::runtime_error(message);
    }
    return true;
}


static UIStyle resolve_style(const std::string& id, const std::string& styleName = "") {
    UIStyle base = g_current_vm->ui_state.defaultStyle;
    if (!styleName.empty()) {
        auto sIt = g_current_vm->ui_state.stylesheets.find(styleName);
        if (sIt != g_current_vm->ui_state.stylesheets.end()) {
            base = sIt->second;
        }
    } else if (g_current_vm->ui_state.activeStyle) {
        base = *g_current_vm->ui_state.activeStyle;
    }
    
    auto it = g_current_vm->ui_state.idOverrides.find(id);
    if (it != g_current_vm->ui_state.idOverrides.end()) {
        const auto& props = it->second;
        if (props.bgColor) base.bgColor = *props.bgColor;
        if (props.textColor) base.textColor = *props.textColor;
        if (props.accentColor) base.accentColor = *props.accentColor;
        if (props.borderRadius) base.borderRadius = *props.borderRadius;
        if (props.fontSize) base.fontSize = *props.fontSize;
        if (props.padding) base.padding = *props.padding;
    }
    return base;
}

static SapphireValue io_readline_native(int arg_count, SapphireValue* args) {
    if (arg_count != 0) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: IO.readLine() expects 0 arguments." << std::endl;
        }
        return {};
    }
    std::string line;
    std::getline(std::cin, line);
    return new_string(g_current_vm, line);
}

static SapphireValue native_io_write_file(int arg_count, SapphireValue* args) {
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

static SapphireValue native_io_read_file(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;

    std::ifstream file(path);
    if (!file.is_open()) return {};

    std::stringstream buffer;
    buffer << file.rdbuf();
    return new_string(g_current_vm, buffer.str());
}

static SapphireValue native_io_open_file_dialog(int arg_count, SapphireValue* args) {
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
#endif
    return new_string(g_current_vm, "");
}

static SapphireValue native_io_exists(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::ifstream file(path);
    return file.good();
}

static SapphireValue native_io_print_color(int arg_count, SapphireValue* args) {
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

static SapphireValue native_io_read_input(int arg_count, SapphireValue* args) {
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

static SapphireValue native_io_delete_file(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    return std::remove(path.c_str()) == 0;
}

static SapphireValue native_io_append_file(int arg_count, SapphireValue* args) {
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

static SapphireValue native_math_sqrt(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: sqrt() expects 1 argument." << std::endl;
        }
        return {};
    }
    if (args[0].type != ValType::VAL_NUMBER) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Argument for sqrt() must be a number." << std::endl;
        }
        return {};
    }
    double number = args[0].as.number;
    return sqrt(number);
}





static SapphireValue native_string_char_at(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || args[1].type != ValType::VAL_NUMBER) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: stringCharAt() expects a string and a number (index)." << std::endl;
        }
        return {};
    }

    ObjString* str_obj = static_cast<ObjString*>(args[0].as.obj);
    int index = static_cast<int>(args[1].as.number);

    if (index < 0 || index >= str_obj->chars.length()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Index out of bounds for string." << std::endl;
        }
        return {};
    }

    return new_string(g_current_vm, std::string(1, str_obj->chars[index]));
}

static SapphireValue native_string_length(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return SapphireValue(0.0);
    }
    ObjString* str_obj = static_cast<ObjString*>(args[0].as.obj);
    return SapphireValue((double)str_obj->chars.length());
}

static SapphireValue native_string_substring(int arg_count, SapphireValue* args) {
    if (arg_count != 3 || !is_obj_type(args[0], OBJ_STRING) || 
        args[1].type != ValType::VAL_NUMBER || 
        args[2].type != ValType::VAL_NUMBER) {
        return new_string(g_current_vm, "");
    }
    ObjString* str_obj = static_cast<ObjString*>(args[0].as.obj);
    int start = static_cast<int>(args[1].as.number);
    int len = static_cast<int>(args[2].as.number);
    
    if (start < 0) start = 0;
    if (start >= str_obj->chars.length()) return new_string(g_current_vm, "");
    if (len < 0) len = 0;
    
    return new_string(g_current_vm, str_obj->chars.substr(start, len));
}

static SapphireValue native_string_split(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) {
        auto arr = new_array(g_current_vm);
        return SapphireValue(arr);
    }
    ObjString* str_obj = static_cast<ObjString*>(args[0].as.obj);
    ObjString* delim_obj = static_cast<ObjString*>(args[1].as.obj);
    
    auto arr = new_array(g_current_vm);
    std::string s = str_obj->chars;
    std::string delim = delim_obj->chars;
    
    if (delim.empty()) {
        for (char c : s) {
            arr->elements.push_back(new_string(g_current_vm, std::string(1, c)));
        }
        return SapphireValue(arr);
    }
    
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delim)) != std::string::npos) {
        token = s.substr(0, pos);
        arr->elements.push_back(new_string(g_current_vm, token));
        s.erase(0, pos + delim.length());
    }
    arr->elements.push_back(new_string(g_current_vm, s));
    
    return SapphireValue(arr);
}

static SapphireValue native_string_replace(int arg_count, SapphireValue* args) {
    if (arg_count != 3 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING) || !is_obj_type(args[2], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) std::cerr << "Runtime Error: stringReplace expects 3 string arguments." << std::endl;
        return {};
    }
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string search = static_cast<ObjString*>(args[1].as.obj)->chars;
    std::string replace = static_cast<ObjString*>(args[2].as.obj)->chars;
    if (search.empty()) return new_string(g_current_vm, s);
    size_t pos = 0;
    while ((pos = s.find(search, pos)) != std::string::npos) {
        s.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return new_string(g_current_vm, s);
}

static SapphireValue native_string_to_upper(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return new_string(g_current_vm, s);
}

static SapphireValue native_string_to_lower(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return new_string(g_current_vm, s);
}

static SapphireValue native_string_trim(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    return new_string(g_current_vm, s);
}

static SapphireValue native_string_contains(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return SapphireValue(false);
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string search = static_cast<ObjString*>(args[1].as.obj)->chars;
    return SapphireValue(s.find(search) != std::string::npos);
}

static std::string valueToStringC(const SapphireValue& val) {
    std::stringstream ss;
    if (val.type == ValType::VAL_NIL) {
        ss << "nil";
    } else if (val.type == ValType::VAL_BOOL) {
        ss << (val.as.boolean ? "true" : "false");
    } else if (val.type == ValType::VAL_NUMBER) {
        double d = val.as.number;
        double int_part;
        if (modf(d, &int_part) == 0.0) ss << static_cast<long long>(d);
        else ss << d;
    } else if (val.type == ValType::VAL_OBJ) {
        Obj* obj = val.as.obj;
        if (obj->type == OBJ_STRING) ss << static_cast<ObjString*>(obj)->chars;
        else if (obj->type == OBJ_MAP) {
            ObjMap* map = static_cast<ObjMap*>(obj);
            ss << "{";
            auto it = map->items.begin();
            while (it != map->items.end()) {
                ss << it->first << ": " << valueToStringC(it->second);
                if (std::next(it) != map->items.end()) ss << ", ";
                ++it;
            }
            ss << "}";
        }
        else ss << "[object]";
    } else if (is_obj_type(val, OBJ_ARRAY)) {
        auto arr = static_cast<ObjArray*>(val.as.obj);
        ss << "[";
        for (size_t i = 0; i < arr->elements.size(); ++i) {
            ss << valueToStringC(arr->elements[i]);
            if (i < arr->elements.size() - 1) ss << ", ";
        }
        ss << "]";
    } else {
        ss << "[unknown]";
    }
    return ss.str();
}

static double valueToDoubleC(const SapphireValue& val) {
    if (val.type == ValType::VAL_NUMBER) {
        return val.as.number;
    } else if (val.type == ValType::VAL_OBJ) {
        ObjString* str = static_cast<ObjString*>(val.as.obj);
        try { return std::stod(str->chars); } catch (...) { return 0.0; }
    }
    return 0.0;
}

static SapphireValue native_lru_create(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) {
        return SapphireValue(new_lru(g_current_vm, 128));
    }
    return SapphireValue(new_lru(g_current_vm, (int)args[0].as.number));
}

static SapphireValue native_lru_has(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_LRU || args[1].type != ValType::VAL_OBJ || args[1].as.obj->type != OBJ_STRING) return SapphireValue(false);
    ObjLRU* lru = (ObjLRU*)args[0].as.obj;
    ObjString* key = (ObjString*)args[1].as.obj;
    return SapphireValue(lru->items.find(key->chars) != lru->items.end());
}

static SapphireValue native_lru_get(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_LRU || args[1].type != ValType::VAL_OBJ || args[1].as.obj->type != OBJ_STRING) return SapphireValue();
    ObjLRU* lru = (ObjLRU*)args[0].as.obj;
    ObjString* key = (ObjString*)args[1].as.obj;
    auto it = lru->items.find(key->chars);
    if (it != lru->items.end()) {
        lru->order.remove(key->chars);
        lru->order.push_front(key->chars);
        return it->second;
    }
    return SapphireValue();
}

static SapphireValue native_lru_put(int arg_count, SapphireValue* args) {
    if (arg_count != 3 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_LRU || args[1].type != ValType::VAL_OBJ || args[1].as.obj->type != OBJ_STRING) return SapphireValue(false);
    ObjLRU* lru = (ObjLRU*)args[0].as.obj;
    ObjString* key = (ObjString*)args[1].as.obj;
    SapphireValue val = args[2];
    
    if (lru->items.find(key->chars) != lru->items.end()) {
        lru->order.remove(key->chars);
    } else {
        if (lru->items.size() >= (size_t)lru->capacity) {
            std::string last = lru->order.back();
            lru->order.pop_back();
            lru->items.erase(last);
        }
    }
    lru->order.push_front(key->chars);
    lru->items[key->chars] = val;
    return SapphireValue(true);
}

static SapphireValue native_value_to_string(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: valueToString() expects 1 argument." << std::endl;
        }
        return new_string(g_current_vm, "");
    }
    return new_string(g_current_vm, valueToStringC(args[0]));
}

static void sapphire_ui_trace(const std::string& id, sf::Vector2f size, float radius) {
    g_current_vm->ui_state.lastComponentId = id;
}

static sf::Color hexToColor(std::string hex) {
    if (hex[0] == '#') hex.erase(0, 1);
    if (hex.length() == 8) {
        uint32_t value = std::stoul(hex, nullptr, 16);
        return sf::Color((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    }
    if (hex.length() != 6) return sf::Color::White;
    uint32_t value = std::stoul(hex, nullptr, 16);
    return sf::Color((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
}

static UIStyle* get_style() {
    return g_current_vm->ui_state.activeStyle ? g_current_vm->ui_state.activeStyle : &g_current_vm->ui_state.defaultStyle;
}

static void sapphire_render_text(sf::RenderWindow& window, const std::string& content, sf::Vector2f pos, sf::Color color, std::string fontAlias, int fontSize) {
    UIStyle* s = get_style();

    std::string finalAlias = (fontAlias != "") ? fontAlias : s->fontAlias;
    unsigned int finalSize = (fontSize > 0) ? (unsigned int)fontSize : s->fontSize;

    if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) {
        finalAlias = "default";
    }

    sf::Text text(g_current_vm->ui_state.fontStack[finalAlias], content, finalSize);
    text.setFillColor(color);
    text.setPosition(pos);
    window.draw(text);
}

static void draw_rounded_rect(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, float radius, sf::Color color, sf::Color outline, float thickness) {
    if (size.x <= 0.0f || size.y <= 0.0f) return;

    float maxRadius = std::min(size.x, size.y) * 0.5f;
    float safeRadius = std::max(0.0f, std::min(radius, maxRadius));

    if (safeRadius < 0.5f) {
        sf::RectangleShape rect(size);
        rect.setPosition(pos);
        rect.setFillColor(color);
        rect.setOutlineColor(outline);
        rect.setOutlineThickness(thickness);
        window.draw(rect);
        return;
    }

    const size_t pointsPerCorner = 10;
    std::vector<sf::Vector2f> pts;
    pts.reserve(pointsPerCorner * 4);

    auto addUniquePoint = [&](sf::Vector2f p) {
        if (pts.empty()) {
            pts.push_back(p);
        } else {
            sf::Vector2f last = pts.back();
            float dx = p.x - last.x;
            float dy = p.y - last.y;
            if (dx * dx + dy * dy > 0.0001f) {
                pts.push_back(p);
            }
        }
    };

    const float PI_2 = 1.570796f;
    for (size_t i = 0; i < pointsPerCorner; i++) {
        float a = (float)i * PI_2 / (pointsPerCorner - 1);
        addUniquePoint({size.x - safeRadius + std::cos(a) * safeRadius, size.y - safeRadius + std::sin(a) * safeRadius});
    }
    for (size_t i = 0; i < pointsPerCorner; i++) {
        float a = (float)i * PI_2 / (pointsPerCorner - 1);
        addUniquePoint({safeRadius - std::sin(a) * safeRadius, size.y - safeRadius + std::cos(a) * safeRadius});
    }
    for (size_t i = 0; i < pointsPerCorner; i++) {
        float a = (float)i * PI_2 / (pointsPerCorner - 1);
        addUniquePoint({safeRadius - std::cos(a) * safeRadius, safeRadius - std::sin(a) * safeRadius});
    }
    for (size_t i = 0; i < pointsPerCorner; i++) {
        float a = (float)i * PI_2 / (pointsPerCorner - 1);
        addUniquePoint({size.x - safeRadius + std::sin(a) * safeRadius, safeRadius - std::cos(a) * safeRadius});
    }

    if (pts.size() > 1) {
        sf::Vector2f first = pts[0];
        sf::Vector2f last = pts.back();
        float dx = first.x - last.x;
        float dy = first.y - last.y;
        if (dx * dx + dy * dy < 0.0001f) pts.pop_back();
    }

    if (pts.size() < 3) return;

    sf::ConvexShape shape(pts.size());
    for (size_t i = 0; i < pts.size(); i++) shape.setPoint(i, pts[i]);

    shape.setFillColor(color);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(thickness);
    shape.setPosition(pos);
    window.draw(shape);
}
static void draw_element_box(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, sf::Color color, sf::Color outline) {
    UIStyle* s = get_style();
    draw_rounded_rect(window, pos, size, s->borderRadius, color, outline, s->borderThickness);
}







static SapphireValue create_declarative_node(const std::string& type, int arg_count, SapphireValue* args) {
    ObjInstance* node = new_instance(g_current_vm, g_current_vm->ui_component_class);
    node->fields["type"] = new_string(g_current_vm, type);
    
    if (arg_count > 0 && is_obj_type(args[0], OBJ_MAP)) {
        ObjMap* map = static_cast<ObjMap*>(args[0].as.obj);
        for (auto& pair : map->items) {
            node->fields[pair.first] = pair.second;
        }
    }
    // Check if the first argument is positional (not named), just in case backward compatibility is needed
    else if (arg_count > 0 && !is_obj_type(args[0], OBJ_NAMED_ARG)) {
        if (type == "Text" || type == "Display") node->fields["text"] = new_string(g_current_vm, valueToStringC(args[0]));
        else node->fields["label"] = new_string(g_current_vm, valueToStringC(args[0]));
    }
    
    // Parse all named arguments (overriding positional if specified)
    for (int i = 0; i < arg_count; i++) {
        if (is_obj_type(args[i], OBJ_NAMED_ARG)) {
            ObjNamedArg* narg = static_cast<ObjNamedArg*>(args[i].as.obj);
            node->fields[narg->name->chars] = narg->value;
        }
    }
    return node;
}

static SapphireValue native_ui_button(int arg_count, SapphireValue* args) { return create_declarative_node("Button", arg_count, args); }
static SapphireValue native_ui_text(int arg_count, SapphireValue* args) { return create_declarative_node("Text", arg_count, args); }
static SapphireValue native_ui_display(int arg_count, SapphireValue* args) { return create_declarative_node("Display", arg_count, args); }
static SapphireValue native_ui_flex(int arg_count, SapphireValue* args) { return create_declarative_node("Container", arg_count, args); }
static SapphireValue native_ui_checkbox(int arg_count, SapphireValue* args) { return create_declarative_node("Checkbox", arg_count, args); }
static SapphireValue native_ui_slider(int arg_count, SapphireValue* args) { return create_declarative_node("Slider", arg_count, args); }
static SapphireValue native_ui_input(int arg_count, SapphireValue* args) { return create_declarative_node("Input", arg_count, args); }
static SapphireValue native_ui_separator(int arg_count, SapphireValue* args) { return create_declarative_node("Separator", arg_count, args); }
static SapphireValue native_ui_menu(int arg_count, SapphireValue* args) { return create_declarative_node("Menu", arg_count, args); }
static SapphireValue native_ui_menuitem(int arg_count, SapphireValue* args) { return create_declarative_node("MenuItem", arg_count, args); }

// Advanced & Layouts
static SapphireValue native_ui_grid(int arg_count, SapphireValue* args) { return create_declarative_node("Grid", arg_count, args); }
static SapphireValue native_ui_stackpanel(int arg_count, SapphireValue* args) { return create_declarative_node("StackPanel", arg_count, args); }
static SapphireValue native_ui_dockpanel(int arg_count, SapphireValue* args) { return create_declarative_node("DockPanel", arg_count, args); }
static SapphireValue native_ui_wrappanel(int arg_count, SapphireValue* args) { return create_declarative_node("WrapPanel", arg_count, args); }
static SapphireValue native_ui_scrollview(int arg_count, SapphireValue* args) { return create_declarative_node("ScrollView", arg_count, args); }
static SapphireValue native_ui_border(int arg_count, SapphireValue* args) { return create_declarative_node("Border", arg_count, args); }

// Controls
static SapphireValue native_ui_image(int arg_count, SapphireValue* args) { return create_declarative_node("Image", arg_count, args); }
static SapphireValue native_ui_progressbar(int arg_count, SapphireValue* args) { return create_declarative_node("ProgressBar", arg_count, args); }
static SapphireValue native_ui_radiobox(int arg_count, SapphireValue* args) { return create_declarative_node("RadioBox", arg_count, args); }
static SapphireValue native_ui_toggleswitch(int arg_count, SapphireValue* args) { return create_declarative_node("ToggleSwitch", arg_count, args); }
static SapphireValue native_ui_combobox(int arg_count, SapphireValue* args) { return create_declarative_node("ComboBox", arg_count, args); }
static SapphireValue native_ui_listbox(int arg_count, SapphireValue* args) { return create_declarative_node("ListBox", arg_count, args); }
static SapphireValue native_ui_passwordbox(int arg_count, SapphireValue* args) { return create_declarative_node("PasswordBox", arg_count, args); }
static SapphireValue native_ui_hyperlink(int arg_count, SapphireValue* args) { return create_declarative_node("Hyperlink", arg_count, args); }
static SapphireValue native_ui_expander(int arg_count, SapphireValue* args) { return create_declarative_node("Expander", arg_count, args); }

// Specialized
static SapphireValue native_ui_datagrid(int arg_count, SapphireValue* args) { return create_declarative_node("DataGrid", arg_count, args); }
static SapphireValue native_ui_canvas(int arg_count, SapphireValue* args) { return create_declarative_node("Canvas", arg_count, args); }
static SapphireValue native_ui_tooltip(int arg_count, SapphireValue* args) { return create_declarative_node("Tooltip", arg_count, args); }
static SapphireValue native_ui_popup(int arg_count, SapphireValue* args) { return create_declarative_node("Popup", arg_count, args); }
static SapphireValue native_ui_window(int arg_count, SapphireValue* args) { return create_declarative_node("Window", arg_count, args); }

static SapphireValue native_ui_animate(int arg_count, SapphireValue* args) {
    std::cout << "native_ui_animate called with arg_count=" << arg_count << std::endl;
    if (arg_count < 2) return false;
    if (!is_obj_type(args[0], OBJ_STRING)) { std::cout << "arg0 not string" << std::endl; return false; }
    if (!is_obj_type(args[1], OBJ_MAP)) { std::cout << "arg1 not map" << std::endl; return false; }

    std::string id = static_cast<ObjString*>(args[0].as.obj)->chars;
    ObjMap* dict = static_cast<ObjMap*>(args[1].as.obj);
    std::cout << "native_ui_animate id=" << id << std::endl;

    Animation anim;
    anim.id = id;
    if (dict->items.count("duration") && dict->items["duration"].type == ValType::VAL_NUMBER)
        anim.duration = (float)dict->items["duration"].as.number;
    
    if (dict->items.count("loop") && dict->items["loop"].type == ValType::VAL_BOOL)
        anim.loop = dict->items["loop"].as.boolean;
    
    if (dict->items.count("easing") && is_obj_type(dict->items["easing"], OBJ_STRING))
        anim.easing = static_cast<ObjString*>(dict->items["easing"].as.obj)->chars;
    
    if (dict->items.count("keyframes") && is_obj_type(dict->items["keyframes"], OBJ_ARRAY)) {
        auto kfs = static_cast<ObjArray*>(dict->items["keyframes"].as.obj);
        for (auto& kfVal : kfs->elements) {
            if (is_obj_type(kfVal, OBJ_MAP)) {
                ObjMap* kfInst = static_cast<ObjMap*>(kfVal.as.obj);
                Keyframe kf;
                if (kfInst->items.count("time") && kfInst->items["time"].type == ValType::VAL_NUMBER)
                    kf.timeOffset = (float)kfInst->items["time"].as.number;
                
                for (auto& [k, v] : kfInst->items) {
                    if (k == "time") continue;
                    if (v.type == ValType::VAL_NUMBER) {
                        kf.numericProps[k] = (float)v.as.number;
                        std::cout << "Parsed numeric prop: " << k << " = " << kf.numericProps[k] << std::endl;
                    } else if (is_obj_type(v, OBJ_STRING)) {
                        kf.colorProps[k] = hexToColor(static_cast<ObjString*>(v.as.obj)->chars);
                        std::cout << "Parsed color prop: " << k << std::endl;
                    } else {
                        std::cout << "Keyframe value for " << k << " has unknown type." << std::endl;
                    }
                }
                anim.keyframes.push_back(kf);
            } else if (is_obj_type(kfVal, OBJ_INSTANCE)) {
                ObjInstance* kfInst = static_cast<ObjInstance*>(kfVal.as.obj);
                Keyframe kf;
                if (kfInst->fields.count("time") && kfInst->fields["time"].type == ValType::VAL_NUMBER)
                    kf.timeOffset = (float)kfInst->fields["time"].as.number;
                
                for (auto& [k, v] : kfInst->fields) {
                    if (k == "time") continue;
                    if (v.type == ValType::VAL_NUMBER) {
                        kf.numericProps[k] = (float)v.as.number;
                        std::cout << "Parsed numeric prop (instance): " << k << " = " << kf.numericProps[k] << std::endl;
                    } else if (is_obj_type(v, OBJ_STRING)) {
                        kf.colorProps[k] = hexToColor(static_cast<ObjString*>(v.as.obj)->chars);
                        std::cout << "Parsed color prop (instance): " << k << std::endl;
                    } else {
                        std::cout << "Keyframe instance value for " << k << " has unknown type." << std::endl;
                    }
                }
                anim.keyframes.push_back(kf);
            } else {
                std::cout << "kfVal is NOT OBJ_MAP or OBJ_INSTANCE. type index: " << (int)kfVal.type << std::endl;
            }
        }
    }

    g_current_vm->ui_state.animations[id] = anim;
    
    ActiveAnimation aa;
    aa.animId = id;
    aa.elapsedTime = 0.0f;
    g_current_vm->ui_state.activeAnimations[id] = aa;

    return true;
}

static SapphireValue native_ui_style(int arg_count, SapphireValue* args) {
    if (arg_count == 0) return {};
    
    if (arg_count == 1 && is_obj_type(args[0], OBJ_STRING)) {
        std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[SAPPHIRE ERROR] Could not open style file: " << path << std::endl;
            return {};
        }
        nlohmann::json j;
        file >> j;
        
        for (auto& [styleName, styleData] : j.items()) {
            UIStyle style = g_current_vm->ui_state.defaultStyle;
            if (styleData.contains("bgColor")) style.bgColor = hexToColor(styleData["bgColor"].get<std::string>());
            if (styleData.contains("textColor")) style.textColor = hexToColor(styleData["textColor"].get<std::string>());
            if (styleData.contains("accentColor")) style.accentColor = hexToColor(styleData["accentColor"].get<std::string>());
            if (styleData.contains("hoverColor")) style.hoverColor = hexToColor(styleData["hoverColor"].get<std::string>());
            if (styleData.contains("borderColor")) style.borderColor = hexToColor(styleData["borderColor"].get<std::string>());
            if (styleData.contains("borderThickness")) style.borderThickness = styleData["borderThickness"].get<float>();
            if (styleData.contains("borderRadius")) style.borderRadius = styleData["borderRadius"].get<float>();
            if (styleData.contains("padding")) style.padding = styleData["padding"].get<float>();
            if (styleData.contains("fontAlias")) style.fontAlias = styleData["fontAlias"].get<std::string>();
            if (styleData.contains("fontSize")) style.fontSize = styleData["fontSize"].get<unsigned int>();
            if (styleData.contains("width")) style.width = styleData["width"].get<float>();
            if (styleData.contains("height")) style.height = styleData["height"].get<float>();
            if (styleData.contains("margin")) style.margin = styleData["margin"].get<float>();
            if (styleData.contains("thickness")) style.thickness = styleData["thickness"].get<float>();
            
            g_current_vm->ui_state.stylesheets[styleName] = style;
        }
    } else if (arg_count > 0) {
        std::string styleName = "";
        if (!is_obj_type(args[0], OBJ_NAMED_ARG) && is_obj_type(args[0], OBJ_STRING)) {
            styleName = static_cast<ObjString*>(args[0].as.obj)->chars;
        }
        UIStyle style = g_current_vm->ui_state.defaultStyle;
        for (int i = 0; i < arg_count; i++) {
            if (is_obj_type(args[i], OBJ_NAMED_ARG)) {
                ObjNamedArg* narg = static_cast<ObjNamedArg*>(args[i].as.obj);
                std::string key = narg->name->chars;
                if (key == "name" && is_obj_type(narg->value, OBJ_STRING)) styleName = static_cast<ObjString*>(narg->value.as.obj)->chars;
                else if (key == "bgColor" && is_obj_type(narg->value, OBJ_STRING)) style.bgColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "textColor" && is_obj_type(narg->value, OBJ_STRING)) style.textColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "accentColor" && is_obj_type(narg->value, OBJ_STRING)) style.accentColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "hoverColor" && is_obj_type(narg->value, OBJ_STRING)) style.hoverColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "borderColor" && is_obj_type(narg->value, OBJ_STRING)) style.borderColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "borderThickness" && narg->value.type == ValType::VAL_NUMBER) style.borderThickness = (float)narg->value.as.number;
                else if (key == "borderRadius" && narg->value.type == ValType::VAL_NUMBER) style.borderRadius = (float)narg->value.as.number;
                else if (key == "padding" && narg->value.type == ValType::VAL_NUMBER) style.padding = (float)narg->value.as.number;
                else if (key == "fontAlias" && is_obj_type(narg->value, OBJ_STRING)) style.fontAlias = static_cast<ObjString*>(narg->value.as.obj)->chars;
                else if (key == "fontSize" && narg->value.type == ValType::VAL_NUMBER) style.fontSize = (unsigned int)narg->value.as.number;
                else if (key == "width" && narg->value.type == ValType::VAL_NUMBER) style.width = (float)narg->value.as.number;
                else if (key == "height" && narg->value.type == ValType::VAL_NUMBER) style.height = (float)narg->value.as.number;
                else if (key == "margin" && narg->value.type == ValType::VAL_NUMBER) style.margin = (float)narg->value.as.number;
                else if (key == "thickness" && narg->value.type == ValType::VAL_NUMBER) style.thickness = (float)narg->value.as.number;
            }
        }
        if (!styleName.empty()) {
            g_current_vm->ui_state.stylesheets[styleName] = style;
        }
    }
    return {};
}

static void compute_sizes(std::shared_ptr<UINode> node) {
    if (!node) return;
    
    for (auto& child : node->children) {
        compute_sizes(child);
    }
    
    bool isContainer = (node->type == UINodeType::Container || node->type == UINodeType::Window || node->type == UINodeType::StackPanel || node->type == UINodeType::Border || node->type == UINodeType::Grid || node->type == UINodeType::WrapPanel || node->type == UINodeType::DockPanel || node->type == UINodeType::ScrollView || node->type == UINodeType::Canvas);
    if (isContainer) {
        float maxChildWidth = 0.0f;
        float maxChildHeight = 0.0f;
        float sumWidth = 0.0f;
        float sumHeight = 0.0f;
        
        for (auto& child : node->children) {
            if (child->width > maxChildWidth) maxChildWidth = child->width;
            if (child->height > maxChildHeight) maxChildHeight = child->height;
            sumWidth += child->width;
            sumHeight += child->height;
        }
        
        float totalGap = node->children.empty() ? 0 : (node->children.size() - 1) * node->gap;
        
        if (node->width <= 0) {
            if (node->direction == "row") node->width = sumWidth + totalGap;
            else node->width = maxChildWidth;
        }
        if (node->height <= 0) {
            if (node->direction == "column") node->height = sumHeight + totalGap;
            else node->height = maxChildHeight;
        }
    }
}

static void place_children(std::shared_ptr<UINode> node, float startX, float startY) {
    if (!node) return;
    node->x = startX;
    node->y = startY;
    
    bool isContainer = (node->type == UINodeType::Container || node->type == UINodeType::Window || node->type == UINodeType::StackPanel || node->type == UINodeType::Border || node->type == UINodeType::Grid || node->type == UINodeType::WrapPanel || node->type == UINodeType::DockPanel || node->type == UINodeType::ScrollView || node->type == UINodeType::Canvas);
    if (isContainer) {
        float currentX = startX;
        float currentY = startY;
        
        float freeSpaceX = node->width;
        float freeSpaceY = node->height;
        float totalGap = node->children.empty() ? 0 : (node->children.size() - 1) * node->gap;
        
        for (auto& child : node->children) {
            if (node->direction == "row") freeSpaceX -= child->width;
            else freeSpaceY -= child->height;
        }
        if (node->direction == "row") freeSpaceX -= totalGap;
        else freeSpaceY -= totalGap;
        
        float gapExtraX = 0;
        float gapExtraY = 0;
        
        if (node->direction == "row") {
            if (node->justify == "center") currentX += freeSpaceX / 2.0f;
            else if (node->justify == "flex-end") currentX += freeSpaceX;
            else if (node->justify == "space-between" && node->children.size() > 1) gapExtraX = freeSpaceX / (node->children.size() - 1);
        } else {
            if (node->justify == "center") currentY += freeSpaceY / 2.0f;
            else if (node->justify == "flex-end") currentY += freeSpaceY;
            else if (node->justify == "space-between" && node->children.size() > 1) gapExtraY = freeSpaceY / (node->children.size() - 1);
        }
        
        for (auto& child : node->children) {
            float childX = currentX;
            float childY = currentY;
            
            if (node->direction == "row") {
                if (node->align == "stretch") {
                    child->height = node->height;
                } else if (node->align == "center") {
                    childY = startY + (node->height / 2.0f) - (child->height / 2.0f);
                } else if (node->align == "flex-end") {
                    childY = startY + node->height - child->height;
                }
            } else {
                if (node->align == "stretch") {
                    child->width = node->width;
                } else if (node->align == "center") {
                    childX = startX + (node->width / 2.0f) - (child->width / 2.0f);
                } else if (node->align == "flex-end") {
                    childX = startX + node->width - child->width;
                }
            }
            
            place_children(child, childX, childY);
            
            if (node->direction == "row") {
                currentX += child->width + node->gap + gapExtraX;
            } else {
                currentY += child->height + node->gap + gapExtraY;
            }
        }
    } else if (node->type == UINodeType::Menu) {
        float currentY = startY + node->height;
        for (auto& child : node->children) {
            place_children(child, startX, currentY);
            currentY += child->height;
        }
    }
}

static void hit_test_tree(std::shared_ptr<UINode> node, sf::Vector2i m, bool mouseJustClicked) {
    if (!node) return;
    
    bool hovered = sf::FloatRect({node->x, node->y}, {node->width, node->height}).contains(sf::Vector2f((float)m.x, (float)m.y));
    g_current_vm->ui_state.hoverState[node->id] = hovered;
    
    if (hovered && mouseJustClicked) {
        if (node->type == UINodeType::Input) {
            g_current_vm->ui_state.focusedInputId = node->id;
            // Calculate cursor position from click X
            const std::string& text = g_current_vm->ui_state.inputTexts[node->id];
            std::string finalAlias = "default";
            // Find best cursor position by measuring text widths
            UIStyle s = resolve_style(node->id, node->styleName);
            if (!s.fontAlias.empty() && g_current_vm->ui_state.fontStack.count(s.fontAlias))
                finalAlias = s.fontAlias;
            if (!g_current_vm->ui_state.fontStack.count(finalAlias)) finalAlias = "default";
            if (g_current_vm->ui_state.fontStack.count(finalAlias)) {
                auto& font = g_current_vm->ui_state.fontStack[finalAlias];
                float clickX = g_current_vm->ui_state.mouseClickPos.x - node->x - 10.0f;
                // Measure scroll offset same way renderer does
                sf::Text fullTxt(font, text, s.fontSize > 0 ? s.fontSize : 18);
                float textWidth = fullTxt.getLocalBounds().size.x;
                float maxVisible = (node->width > 0 ? node->width : 250.f) - 20.0f;
                float scrollOffset = (textWidth > maxVisible) ? textWidth - maxVisible : 0.0f;
                clickX += scrollOffset;
                size_t bestPos = 0;
                float bestDist = std::abs(clickX);
                for (size_t i = 1; i <= text.size(); i++) {
                    sf::Text t(font, text.substr(0, i), s.fontSize > 0 ? s.fontSize : 18);
                    float cx = t.getLocalBounds().size.x;
                    float dist = std::abs(clickX - cx);
                    if (dist < bestDist) { bestDist = dist; bestPos = i; }
                }
                g_current_vm->ui_state.cursorPositions[node->id] = bestPos;
            } else {
                g_current_vm->ui_state.cursorPositions[node->id] = text.length();
            }
        } else if (node->type == UINodeType::Button || node->type == UINodeType::Checkbox || node->type == UINodeType::RadioBox || node->type == UINodeType::ToggleSwitch || node->type == UINodeType::Slider || node->type == UINodeType::MenuItem) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastClickTime);
            if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
                g_current_vm->ui_state.lastClickTime = now;
                g_current_vm->ui_state.clickState[node->id] = true;
                if (node->type == UINodeType::Checkbox || node->type == UINodeType::RadioBox || node->type == UINodeType::ToggleSwitch) {
                    g_current_vm->ui_state.toggleStates[node->id] = !g_current_vm->ui_state.toggleStates[node->id];
                }
                g_current_vm->ui_state.focusedInputId = "";
                g_current_vm->ui_state.activeMenu = ""; // Close menu on option click
            }
        } else if (node->type == UINodeType::Hyperlink) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastClickTime);
            if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
                g_current_vm->ui_state.lastClickTime = now;
                g_current_vm->ui_state.clickState[node->id] = true;
                if (!node->href.empty()) {
                    #if defined(_WIN32)
                        system(("start \"\" \"" + node->href + "\"").c_str());
                    #elif defined(__APPLE__)
                        system(("open " + node->href).c_str());
                    #else
                        system(("xdg-open " + node->href).c_str());
                    #endif
                }
            }
        } else if (node->type == UINodeType::Menu) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastClickTime);
            if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
                g_current_vm->ui_state.lastClickTime = now;
                g_current_vm->ui_state.clickState[node->id] = true;
                if (g_current_vm->ui_state.activeMenu == node->id) g_current_vm->ui_state.activeMenu = "";
                else g_current_vm->ui_state.activeMenu = node->id;
                g_current_vm->ui_state.focusedInputId = "";
            }
        }
    }
    
    if (node->type != UINodeType::Menu || g_current_vm->ui_state.activeMenu == node->id) {
        for (auto& child : node->children) {
            hit_test_tree(child, m, mouseJustClicked);
        }
    }
}

static std::vector<std::shared_ptr<UINode>> deferred_render_nodes;

static void render_ui_tree(std::shared_ptr<UINode> node) {
    if (!node) return;
    
    if (node->type == UINodeType::Menu && g_current_vm->ui_state.activeMenu == node->id) {
        for (auto& child : node->children) {
            deferred_render_nodes.push_back(child);
        }
    }
    
    UIStyle s = resolve_style(node->id, node->styleName);
    if (!node->customColor.empty()) {
        s.bgColor = hexToColor(node->customColor);
        s.textColor = s.bgColor;
    }
    if (node->fontSize > 0) s.fontSize = node->fontSize;
    if (s.fontSize <= 0) s.fontSize = 18;
    if (!node->fontAlias.empty()) s.fontAlias = node->fontAlias;
    
    bool hovered = g_current_vm->ui_state.hoverState[node->id];
    
    if (node->type == UINodeType::Container) {
        if (s.bgColor.a > 0 || s.borderThickness > 0) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                              s.borderRadius, s.bgColor, s.borderColor, s.borderThickness);
        }
    }
    else if (node->type == UINodeType::Button || node->type == UINodeType::Menu || node->type == UINodeType::MenuItem) {
        sf::Color btnBg = s.bgColor.a == 0 ? s.accentColor : s.bgColor;
        if (hovered) {
            btnBg = sf::Color(std::min(btnBg.r + 30, 255), std::min(btnBg.g + 30, 255), std::min(btnBg.b + 30, 255), btnBg.a);
        }
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                          s.borderRadius, btnBg, s.borderColor, s.borderThickness);
        
        if (!node->label.empty()) {
            std::string finalAlias = s.fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            sf::Font& font = g_current_vm->ui_state.fontStack[finalAlias];
            
            unsigned int actualSize = s.fontSize;
            if (node->width > 0) {
                float maxTextWidth = node->width - (s.padding * 2);
                if (maxTextWidth > 0) {
                    while (actualSize > 8) {
                        sf::Text dummy(font, node->label, actualSize);
                        if (dummy.getLocalBounds().size.x <= maxTextWidth) break;
                        actualSize--;
                    }
                }
            }
            
            sf::Text dummyText(font, node->label, actualSize);
            float tw = dummyText.getLocalBounds().size.x;
            
            float textX = node->x + s.padding;
            if (node->align == "center") {
                textX = node->x + (node->width / 2.0f) - (tw / 2.0f);
            } else if (node->align == "right") {
                textX = node->x + node->width - tw - s.padding;
            } else {
                textX = node->x + (node->width / 2.0f) - (tw / 2.0f); // Default to center for buttons
            }
            
            sf::Text txt(font, node->label, actualSize);
            txt.setFillColor(sf::Color::White); // Buttons look best with white text on accent background
            txt.setPosition({textX, node->y + (node->height / 2.0f) - (actualSize / 2.0f) - 2.0f});
            g_current_vm->sfml_window->draw(txt);
        }
    }

    else if (node->type == UINodeType::Display) {
        if (s.bgColor.a > 0 || s.borderThickness > 0) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                              s.borderRadius, s.bgColor, s.borderColor, s.borderThickness);
        }
        
        std::string finalAlias = s.fontAlias;
        if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
        
        sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, s.fontSize);
        float tw = dummyText.getLocalBounds().size.x;
        
        float textX = node->x + s.padding;
        float textY = node->y + s.padding;
        
        if (node->align == "center") {
            textX = node->x + (node->width / 2.0f) - (tw / 2.0f);
        } else if (node->align == "right" || node->align == "flex-end") {
            textX = node->x + node->width - tw - s.padding;
        }
        
        if (node->justify == "center") {
            textY = node->y + (node->height / 2.0f) - (s.fontSize / 2.0f);
        } else if (node->justify == "bottom" || node->justify == "flex-end") {
            textY = node->y + node->height - s.fontSize - s.padding;
        }

        if (node->shadow) {
            sapphire_render_text(*g_current_vm->sfml_window, node->label,
                                 {textX + 2.0f, textY + 2.0f},
                                 sf::Color(0, 0, 0, 150), s.fontAlias, s.fontSize);
        }
        
        sapphire_render_text(*g_current_vm->sfml_window, node->label,
                             {textX, textY},
                             s.textColor, s.fontAlias, s.fontSize);
    }
    else if (node->type == UINodeType::Text) {
        sapphire_render_text(*g_current_vm->sfml_window, node->label,
                             {node->x, node->y},
                             s.textColor, s.fontAlias, s.fontSize);
    }
    else if (node->type == UINodeType::Checkbox) {
        float size = 20.0f;
        sf::Vector2f pos(node->x, node->y);
        draw_rounded_rect(*g_current_vm->sfml_window, pos, {size, size}, s.borderRadius, hovered ? s.hoverColor : s.bgColor, s.accentColor, s.borderThickness);
        if (node->checked) {
            float offset = 5.0f;
            draw_rounded_rect(*g_current_vm->sfml_window, {pos.x + offset, pos.y + offset}, {size - 10, size - 10}, s.borderRadius * 0.5f, s.accentColor, sf::Color::Transparent, 0);
        }
        if (!node->label.empty()) {
            sapphire_render_text(*g_current_vm->sfml_window, node->label, {pos.x + size + 10.0f, pos.y}, s.textColor, s.fontAlias, s.fontSize);
        }
    }
    else if (node->type == UINodeType::Slider) {
        float width = node->width > 0 ? node->width : 200.0f;
        sf::RectangleShape bar({width, 6.0f});
        bar.setPosition({node->x, node->y + 10.0f});
        bar.setFillColor(s.borderColor);
        g_current_vm->sfml_window->draw(bar);

        float valPos = ((node->value - node->min) / (node->max - node->min)) * width;
        if (std::isnan(valPos) || std::isinf(valPos)) valPos = 0.0f;
        
        sf::CircleShape handle(8.0f);
        handle.setOrigin({8.0f, 8.0f});
        handle.setPosition({node->x + valPos, node->y + 13.0f});
        handle.setFillColor(s.accentColor);
        
        if (hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
            float newPos = std::clamp((float)m.x - node->x, 0.0f, width);
            node->value = node->min + (newPos / width) * (node->max - node->min);
            g_current_vm->ui_state.sliderValues[node->id] = node->value;
        }
        
        g_current_vm->sfml_window->draw(handle);
    }
    else if (node->type == UINodeType::Input) {
        float width = node->width > 0 ? node->width : 250.0f;
        float height = node->height > 0 ? node->height : 35.0f;
        sf::Vector2f pos(node->x, node->y);
        
        bool isFocused = (g_current_vm->ui_state.focusedInputId == node->id);
        
        // Draw border: accent color when focused, normal when not
        sf::Color borderCol = isFocused ? g_current_vm->ui_state.stylesheets.count(node->styleName) ?
            g_current_vm->ui_state.stylesheets[node->styleName].accentColor : sf::Color(0, 122, 204)
            : s.accentColor;
        draw_rounded_rect(*g_current_vm->sfml_window, pos, {width, height}, s.borderRadius, s.bgColor, borderCol, isFocused ? 2.0f : s.borderThickness);

        sf::Vector2u winSize = g_current_vm->sfml_window->getSize();
        if (winSize.x > 0 && winSize.y > 0 && width > 0 && height > 0) {
            sf::View oldView = g_current_vm->sfml_window->getView();
            sf::FloatRect viewportRect({pos.x / (float)winSize.x, pos.y / (float)winSize.y}, {width / (float)winSize.x, height / (float)winSize.y});
            sf::View inputView(sf::FloatRect({0.f, 0.f}, {width, height}));
            inputView.setViewport(viewportRect);
            g_current_vm->sfml_window->setView(inputView);
            
            std::string displayText = node->label;
            size_t cursorPos = g_current_vm->ui_state.cursorPositions.count(node->id)
                               ? g_current_vm->ui_state.cursorPositions[node->id] : 0;
            if (cursorPos > displayText.length()) {
                cursorPos = displayText.length();
                g_current_vm->ui_state.cursorPositions[node->id] = cursorPos;
            }
            
            std::string finalAlias = s.fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            auto& font = g_current_vm->ui_state.fontStack[finalAlias];
            unsigned int fsize = s.fontSize > 0 ? s.fontSize : 18;

            // Compute scroll offset based on cursor position
            sf::Text cursorMeasure(font, displayText.substr(0, cursorPos), fsize);
            float cursorX = cursorMeasure.getLocalBounds().size.x;
            sf::Text fullMeasure(font, displayText, fsize);
            float textWidth = fullMeasure.getLocalBounds().size.x;
            float maxVisible = width - 20.0f;

            // Keep cursor in view
            float scrollOffset = 0.0f;
            if (cursorX > maxVisible) scrollOffset = cursorX - maxVisible;
            else if (textWidth > maxVisible) scrollOffset = 0.0f; // let text extend to left
            
            sf::Text txt(font, displayText, fsize);
            txt.setFillColor(s.textColor);
            txt.setPosition({10.0f - scrollOffset, (height / 2.0f) - (fsize / 2.0f)});
            g_current_vm->sfml_window->draw(txt);
            
            // Draw cursor only when focused
            if (isFocused) {
                float cx = 10.0f + cursorX - scrollOffset;
                sf::RectangleShape cursor({2.0f, (float)fsize + 4.0f});
                cursor.setPosition({cx, (height / 2.0f) - (fsize / 2.0f) - 2.0f});
                cursor.setFillColor(s.textColor);
                g_current_vm->sfml_window->draw(cursor);
            }
            
            g_current_vm->sfml_window->setView(oldView);
        }
    }
    else if (node->type == UINodeType::Separator) {
        sf::RectangleShape line({node->width, node->thickness});
        line.setPosition({node->x, node->y + node->margin});
        line.setFillColor(s.borderColor);
        g_current_vm->sfml_window->draw(line);
    }
    else if (node->type == UINodeType::ProgressBar) {
        float width = node->width > 0 ? node->width : 200.0f;
        float height = node->height > 0 ? node->height : 15.0f;
        sf::Color trackBg = sf::Color(80, 80, 80, 200);
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {width, height}, height / 2.0f, trackBg, sf::Color::Transparent, 0.0f);
        float p = std::clamp(node->progress, 0.0f, 100.0f) / 100.0f;
        if (p > 0.0f) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {std::max(height, width * p), height}, height / 2.0f, s.accentColor, sf::Color::Transparent, 0.0f);
        }
    }
    else if (node->type == UINodeType::ToggleSwitch) {
        float width = node->width > 0 ? node->width : 40.0f;
        float height = node->height > 0 ? node->height : 20.0f;
        sf::Color bg = node->checked ? s.accentColor : sf::Color(100, 100, 100, 200);
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {width, height}, height / 2.0f, bg, sf::Color::Transparent, 0.0f);
        
        float circleRadius = (height / 2.0f) - 2.0f;
        float cx = node->checked ? (node->x + width - circleRadius * 2.0f - 2.0f) : (node->x + 2.0f);
        sf::CircleShape handle(circleRadius);
        handle.setPosition({cx, node->y + 2.0f});
        handle.setFillColor(sf::Color::White);
        
        sf::CircleShape shadow(circleRadius);
        shadow.setPosition({cx, node->y + 3.0f});
        shadow.setFillColor(sf::Color(0, 0, 0, 80));
        g_current_vm->sfml_window->draw(shadow);
        g_current_vm->sfml_window->draw(handle);
    }
    else if (node->type == UINodeType::RadioBox) {
        float size = 20.0f;
        sf::Vector2f pos(node->x, node->y);
        sf::CircleShape outer(size / 2.0f);
        outer.setPosition(pos);
        outer.setFillColor(hovered ? sf::Color(255, 255, 255, 20) : sf::Color::Transparent);
        outer.setOutlineColor(node->checked ? s.accentColor : sf::Color(150, 150, 150));
        outer.setOutlineThickness(2.0f);
        g_current_vm->sfml_window->draw(outer);
        if (node->checked) {
            sf::CircleShape inner(size / 4.0f);
            inner.setPosition({pos.x + size / 4.0f, pos.y + size / 4.0f});
            inner.setFillColor(s.accentColor);
            g_current_vm->sfml_window->draw(inner);
        }
        if (!node->label.empty()) {
            sapphire_render_text(*g_current_vm->sfml_window, node->label, {pos.x + size + 10.0f, pos.y + (size / 2.0f) - (s.fontSize / 2.0f) - 2.0f}, s.textColor, s.fontAlias, s.fontSize);
        }
    }
    else if (node->type == UINodeType::Hyperlink) {
        sf::Color linkColor = hovered ? sf::Color(100, 180, 255) : s.accentColor;
        sapphire_render_text(*g_current_vm->sfml_window, node->label, {node->x, node->y}, linkColor, s.fontAlias, s.fontSize);
        if (hovered) {
            std::string finalAlias = s.fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, s.fontSize > 0 ? s.fontSize : 18);
            float tw = dummyText.getLocalBounds().size.x;
            sf::RectangleShape line({tw, 1.0f});
            line.setPosition({node->x, node->y + (s.fontSize > 0 ? s.fontSize : 18) + 2.0f});
            line.setFillColor(linkColor);
            g_current_vm->sfml_window->draw(line);
        }
    }
    // Generic blocks for layout panels
    else if (node->type == UINodeType::Grid || node->type == UINodeType::StackPanel || node->type == UINodeType::DockPanel || node->type == UINodeType::WrapPanel || node->type == UINodeType::Border || node->type == UINodeType::Canvas || node->type == UINodeType::Window) {
        if (s.bgColor.a > 0 || s.borderThickness > 0) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                              s.borderRadius, s.bgColor, s.borderColor, s.borderThickness);
        }
    }
    
    // We update scale and rotation by using SFML transforms directly if needed, but since we use manual drawing, 
    // it's complex for generic containers. For an MVP animation, we just change X, Y, Width, Height, Opacity.
    
    if (node->type != UINodeType::Menu) {
        for (auto& child : node->children) {
            render_ui_tree(child);
        }
    }
}

static std::shared_ptr<UINode> build_ui_tree(ObjInstance* nodeDict, int& counter) {
    if (!nodeDict) return nullptr;
    if (nodeDict->fields.find("type") == nodeDict->fields.end()) return nullptr;
    
    auto typeVal = nodeDict->fields["type"];
    if (!is_obj_type(typeVal, OBJ_STRING)) return nullptr;
    std::string typeStr = static_cast<ObjString*>(typeVal.as.obj)->chars;
    
    UINodeType type = UINodeType::Container;
    if (typeStr == "Button") type = UINodeType::Button;
    else if (typeStr == "Text") type = UINodeType::Text;
    else if (typeStr == "Display") type = UINodeType::Display;
    else if (typeStr == "Checkbox") type = UINodeType::Checkbox;
    else if (typeStr == "Slider") type = UINodeType::Slider;
    else if (typeStr == "Input") type = UINodeType::Input;
    else if (typeStr == "Separator") type = UINodeType::Separator;
    else if (typeStr == "Menu") type = UINodeType::Menu;
    else if (typeStr == "MenuItem") type = UINodeType::MenuItem;
    else if (typeStr == "Grid") type = UINodeType::Grid;
    else if (typeStr == "StackPanel") type = UINodeType::StackPanel;
    else if (typeStr == "DockPanel") type = UINodeType::DockPanel;
    else if (typeStr == "WrapPanel") type = UINodeType::WrapPanel;
    else if (typeStr == "ScrollView") type = UINodeType::ScrollView;
    else if (typeStr == "Border") type = UINodeType::Border;
    else if (typeStr == "Image") type = UINodeType::Image;
    else if (typeStr == "ProgressBar") type = UINodeType::ProgressBar;
    else if (typeStr == "RadioBox") type = UINodeType::RadioBox;
    else if (typeStr == "ToggleSwitch") type = UINodeType::ToggleSwitch;
    else if (typeStr == "ComboBox") type = UINodeType::ComboBox;
    else if (typeStr == "ListBox") type = UINodeType::ListBox;
    else if (typeStr == "PasswordBox") type = UINodeType::PasswordBox;
    else if (typeStr == "Hyperlink") type = UINodeType::Hyperlink;
    else if (typeStr == "Expander") type = UINodeType::Expander;
    else if (typeStr == "DataGrid") type = UINodeType::DataGrid;
    else if (typeStr == "Canvas") type = UINodeType::Canvas;
    else if (typeStr == "Tooltip") type = UINodeType::Tooltip;
    else if (typeStr == "Popup") type = UINodeType::Popup;
    else if (typeStr == "Window") type = UINodeType::Window;
    
    std::string id = typeStr + "_" + std::to_string(counter++);
    if (nodeDict->fields.count("id") && is_obj_type(nodeDict->fields["id"], OBJ_STRING)) {
        id = static_cast<ObjString*>(nodeDict->fields["id"].as.obj)->chars;
    }
    
#ifdef DEBUG_PRINT_CODE
    std::cout << "[DEBUG build] type=" << typeStr << " id=" << id;
    if (nodeDict->fields.count("text")) {
        std::cout << " text_present=true";
        if (is_obj_type(nodeDict->fields["text"], OBJ_STRING)) {
            std::cout << " text_val=" << static_cast<ObjString*>(nodeDict->fields["text"].as.obj)->chars;
        }
    }
    if (nodeDict->fields.count("label")) {
        std::cout << " label_present=true";
        if (is_obj_type(nodeDict->fields["label"], OBJ_STRING)) {
            std::cout << " label_val=" << static_cast<ObjString*>(nodeDict->fields["label"].as.obj)->chars;
        }
    }
    std::cout << std::endl;
#endif

    auto node = std::make_shared<UINode>(type, id);
    
    auto get_str = [&](const std::string& key, std::string& out) {
        if (nodeDict->fields.count(key) && is_obj_type(nodeDict->fields[key], OBJ_STRING)) {
            out = static_cast<ObjString*>(nodeDict->fields[key].as.obj)->chars;
        }
    };
    auto get_num = [&](const std::string& key, float& out) {
        if (nodeDict->fields.count(key) && nodeDict->fields[key].type == ValType::VAL_NUMBER) {
            out = (float)nodeDict->fields[key].as.number;
        }
    };
    
    auto get_bool = [&](const std::string& key, bool& out) {
        if (nodeDict->fields.count(key) && nodeDict->fields[key].type == ValType::VAL_BOOL) {
            out = nodeDict->fields[key].as.boolean;
        }
    };
    
    get_str("label", node->label);
    get_str("text", node->label);
    get_num("width", node->width);
    get_num("height", node->height);
    get_str("style", node->styleName);
    get_str("align", node->align);
    get_str("justify", node->justify);
    get_str("direction", node->direction);
    get_num("gap", node->gap);
    get_num("value", node->value);
    get_num("min", node->min);
    get_num("max", node->max);
    get_bool("checked", node->checked);
    if (type == UINodeType::Checkbox || type == UINodeType::RadioBox || type == UINodeType::ToggleSwitch) {
        if (g_current_vm->ui_state.toggleStates.find(id) != g_current_vm->ui_state.toggleStates.end()) {
            node->checked = g_current_vm->ui_state.toggleStates[id];
        } else {
            g_current_vm->ui_state.toggleStates[id] = node->checked;
        }
    }
    else if (type == UINodeType::Slider) {
        if (g_current_vm->ui_state.sliderValues.find(id) != g_current_vm->ui_state.sliderValues.end()) {
            node->value = g_current_vm->ui_state.sliderValues[id];
        } else {
            g_current_vm->ui_state.sliderValues[id] = node->value;
        }
    }
    get_bool("shadow", node->shadow);
    get_num("thickness", node->thickness);
    get_num("margin", node->margin);
    get_str("color", node->customColor);
    if (node->customColor.empty()) {
        get_str("customColor", node->customColor);
    }
    get_str("src", node->src);
    get_num("progress", node->progress);
    get_str("href", node->href);
    get_bool("isPassword", node->isPassword);
    get_bool("expanded", node->expanded);
    get_num("opacity", node->opacity);
    get_num("scaleX", node->scaleX);
    get_num("scaleY", node->scaleY);
    get_num("rotation", node->rotation);
    
    float vrow = 0; get_num("row", vrow); node->row = (int)vrow;
    float vcol = 0; get_num("column", vcol); node->column = (int)vcol;
    float vrowS = 0; get_num("rowSpan", vrowS); if (vrowS > 0) node->rowSpan = (int)vrowS;
    float vcolS = 0; get_num("columnSpan", vcolS); if (vcolS > 0) node->columnSpan = (int)vcolS;
    
    get_num("left", node->left);
    get_num("top", node->top);
    get_num("right", node->right);
    get_num("bottom", node->bottom);

    if (nodeDict->fields.count("options") && is_obj_type(nodeDict->fields["options"], OBJ_ARRAY)) {
        auto arr = static_cast<ObjArray*>(nodeDict->fields["options"].as.obj);
        for (auto& val : arr->elements) {
            if (is_obj_type(val, OBJ_STRING)) {
                node->options.push_back(static_cast<ObjString*>(val.as.obj)->chars);
            }
        }
    }
    
    float fsize = 0;
    get_num("size", fsize);
    if (fsize > 0) node->fontSize = (unsigned int)fsize;
    
    if (nodeDict->fields.count("onClick")) {
        auto onClickVal = nodeDict->fields["onClick"];
        if (is_obj_type(onClickVal, OBJ_CLOSURE) || 
            is_obj_type(onClickVal, OBJ_BOUND_METHOD) || 
            is_obj_type(onClickVal, OBJ_NATIVE) || 
            is_obj_type(onClickVal, OBJ_FUNCTION)) {
            g_current_vm->ui_state.clickHandlers[id] = onClickVal;
        }
    }
    
    if (nodeDict->fields.count("onChange")) {
        auto onChangeVal = nodeDict->fields["onChange"];
        if (is_obj_type(onChangeVal, OBJ_CLOSURE) || 
            is_obj_type(onChangeVal, OBJ_BOUND_METHOD) || 
            is_obj_type(onChangeVal, OBJ_NATIVE) || 
            is_obj_type(onChangeVal, OBJ_FUNCTION)) {
            g_current_vm->ui_state.changeHandlers[id] = onChangeVal;
        }
    }
    
    if (type == UINodeType::Input) {
        bool isFocused = (g_current_vm->ui_state.focusedInputId == id);
        if (!isFocused) {
            // Script owns the value when the input is NOT focused.
            // This handles: initial value, Browse button updates, programmatic changes.
            const std::string& scriptVal = node->label;
            if (g_current_vm->ui_state.inputTexts[id] != scriptVal) {
                // Value changed externally â€” accept it and move cursor to end
                g_current_vm->ui_state.cursorPositions[id] = scriptVal.size();
            }
            g_current_vm->ui_state.inputTexts[id] = scriptVal;
        }
        // When focused, C++ owns the state â€” ignore text= from script completely.
        node->label = g_current_vm->ui_state.inputTexts[id];
    }
    
    if (nodeDict->fields.count("children")) {
        auto childrenVal = nodeDict->fields["children"];
        if (is_obj_type(childrenVal, OBJ_ARRAY)) {
            auto arr = static_cast<ObjArray*>(childrenVal.as.obj);
            for (auto& childVal : arr->elements) {
                if (is_obj_type(childVal, OBJ_INSTANCE)) {
                    auto childNode = build_ui_tree(static_cast<ObjInstance*>(childVal.as.obj), counter);
                    if (childNode) {
                        childNode->parent = node.get();
                        node->children.push_back(childNode);
                    }
                }
            }
        }
    }
    if (!node->styleName.empty()) {
        auto sIt = g_current_vm->ui_state.stylesheets.find(node->styleName);
        if (sIt != g_current_vm->ui_state.stylesheets.end()) {
            if (node->width <= 0.0f) node->width = sIt->second.width;
            if (node->height <= 0.0f) node->height = sIt->second.height;
        }
    }

    if (node->width <= 0.0f) {
        if (type == UINodeType::Button || type == UINodeType::Menu || type == UINodeType::MenuItem) {
            node->width = 120.0f;
            if (!node->label.empty()) {
                std::string finalAlias = node->fontAlias.empty() ? "default" : node->fontAlias;
                if (!g_current_vm->ui_state.fontStack.count(finalAlias)) finalAlias = "default";
                if (g_current_vm->ui_state.fontStack.count(finalAlias)) {
                    sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, node->fontSize > 0 ? node->fontSize : 18);
                    float textW = dummyText.getLocalBounds().size.x + 30.0f; // Add horizontal padding
                    if (textW > node->width) node->width = textW;
                }
            }
        }
        else if (type == UINodeType::Checkbox || type == UINodeType::RadioBox) {
            node->width = 20.0f;
            if (!node->label.empty()) {
                std::string finalAlias = node->fontAlias.empty() ? "default" : node->fontAlias;
                if (!g_current_vm->ui_state.fontStack.count(finalAlias)) finalAlias = "default";
                if (g_current_vm->ui_state.fontStack.count(finalAlias)) {
                    sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, node->fontSize > 0 ? node->fontSize : 18);
                    node->width += dummyText.getLocalBounds().size.x + 15.0f;
                }
            }
        }
        else if (type == UINodeType::Slider || type == UINodeType::ProgressBar) node->width = 200.0f;
        else if (type == UINodeType::Input) node->width = 250.0f;
        else if (type == UINodeType::Display) node->width = 200.0f;
        else if (type == UINodeType::Separator) node->width = 100.0f;
        else if (type == UINodeType::ToggleSwitch) node->width = 40.0f;
        else if (type == UINodeType::Text || type == UINodeType::Hyperlink) {
            std::string finalAlias = node->fontAlias.empty() ? "default" : node->fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            unsigned int fsize = node->fontSize > 0 ? node->fontSize : 18;
            if (g_current_vm->ui_state.fontStack.count(finalAlias)) {
                sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, fsize);
                node->width = dummyText.getLocalBounds().size.x;
            } else {
                node->width = node->label.length() * (fsize * 0.6f);
            }
        }
    }
    if (node->height <= 0.0f) {
        if (type == UINodeType::Button || type == UINodeType::Menu || type == UINodeType::MenuItem) node->height = 40.0f;
        else if (type == UINodeType::Checkbox || type == UINodeType::RadioBox) node->height = 20.0f;
        else if (type == UINodeType::Slider) node->height = 20.0f;
        else if (type == UINodeType::ProgressBar) node->height = 15.0f;
        else if (type == UINodeType::Input) node->height = 35.0f;
        else if (type == UINodeType::Display) node->height = 50.0f;
        else if (type == UINodeType::Separator) node->height = 2.0f;
        else if (type == UINodeType::ToggleSwitch) node->height = 20.0f;
        else if (type == UINodeType::Text || type == UINodeType::Hyperlink) {
            unsigned int fsize = node->fontSize > 0 ? node->fontSize : 18;
            node->height = (float)fsize + 4.0f;
        }
    }

    return node;
}

static float lerp_val(float a, float b, float t) { return a + (b - a) * t; }
static sf::Color lerp_color(sf::Color a, sf::Color b, float t) {
    return sf::Color(
        (uint8_t)lerp_val(a.r, b.r, t),
        (uint8_t)lerp_val(a.g, b.g, t),
        (uint8_t)lerp_val(a.b, b.b, t),
        (uint8_t)lerp_val(a.a, b.a, t)
    );
}

static void apply_animations_to_tree(std::shared_ptr<UINode> node, float dt) {
    if (!node) return;
    
    auto it = g_current_vm->ui_state.activeAnimations.find(node->id);
    if (it != g_current_vm->ui_state.activeAnimations.end()) {
        auto& aa = it->second;
        auto animIt = g_current_vm->ui_state.animations.find(aa.animId);
        if (animIt != g_current_vm->ui_state.animations.end()) {
            auto& anim = animIt->second;
            aa.elapsedTime += dt;
            float t = anim.duration > 0 ? (aa.elapsedTime / anim.duration) : 1.0f;
            if (t > 1.0f) {
                if (anim.loop) { aa.elapsedTime = std::fmod(aa.elapsedTime, anim.duration); t = aa.elapsedTime / anim.duration; }
                else t = 1.0f;
            }
            std::cout << "Anim: id=" << node->id << " dt=" << dt << " elapsed=" << aa.elapsedTime << " t=" << t << " width=" << node->width << std::endl;
            
            if (anim.keyframes.size() >= 2) {
                size_t kfIndex = 0;
                for (size_t i = 0; i < anim.keyframes.size() - 1; i++) {
                    if (t >= anim.keyframes[i].timeOffset && t <= anim.keyframes[i+1].timeOffset) {
                        kfIndex = i; break;
                    }
                }
                auto& kf1 = anim.keyframes[kfIndex];
                auto& kf2 = anim.keyframes[kfIndex+1];
                float timeSpan = kf2.timeOffset - kf1.timeOffset;
                float localT = timeSpan > 0 ? ((t - kf1.timeOffset) / timeSpan) : 0.0f;
                
                for (auto& [prop, val1] : kf1.numericProps) {
                    if (kf2.numericProps.count(prop)) {
                        float val2 = kf2.numericProps.at(prop);
                        float interpolated = lerp_val(val1, val2, localT);
                        if (prop == "width") node->width = interpolated;
                        else if (prop == "height") node->height = interpolated;
                        else if (prop == "x") node->x = interpolated;
                        else if (prop == "y") node->y = interpolated;
                        else if (prop == "opacity") node->opacity = interpolated;
                        else if (prop == "scaleX") node->scaleX = interpolated;
                        else if (prop == "scaleY") node->scaleY = interpolated;
                        else if (prop == "rotation") node->rotation = interpolated;
                    }
                }
                for (auto& [prop, c1] : kf1.colorProps) {
                    if (kf2.colorProps.count(prop)) {
                        sf::Color c2 = kf2.colorProps.at(prop);
                        sf::Color interpolated = lerp_color(c1, c2, localT);
                        if (prop == "color") {
                            char buf[10];
                            snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", interpolated.r, interpolated.g, interpolated.b, interpolated.a);
                            node->customColor = buf;
                        }
                    }
                }
            }
        }
    }
    
    for (auto& child : node->children) {
        apply_animations_to_tree(child, dt);
    }
}

static SapphireValue native_ui_get_input_text(int arg_count, SapphireValue* args) {
    if (arg_count == 1 && is_obj_type(args[0], OBJ_STRING)) {
        std::string id = static_cast<ObjString*>(args[0].as.obj)->chars;
        if (g_current_vm->ui_state.inputTexts.count(id)) {
            return new_string(g_current_vm, g_current_vm->ui_state.inputTexts[id]);
        }
    }
    return new_string(g_current_vm, "");
}

static SapphireValue native_ui_render(int arg_count, SapphireValue* args) {
    if (!g_current_vm->sfml_window) return {};
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_INSTANCE)) return {};

    while (const std::optional event = g_current_vm->sfml_window->pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            g_current_vm->sfml_window->close();
            exit(0);
        }
        if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            sf::FloatRect visibleArea({0.f, 0.f}, {(float)resized->size.x, (float)resized->size.y});
            g_current_vm->sfml_window->setView(sf::View(visibleArea));
        }
        // Capture click event (position + flag) for reliable cursor placement
        if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mb->button == sf::Mouse::Button::Left) {
                g_current_vm->ui_state.mouseJustClicked = true;
                g_current_vm->ui_state.mouseClickPos = sf::Vector2f((float)mb->position.x, (float)mb->position.y);
            }
        }
        if (!g_current_vm->ui_state.focusedInputId.empty() && g_current_vm->ui_state.inputTexts.count(g_current_vm->ui_state.focusedInputId)) {
            const std::string& fid = g_current_vm->ui_state.focusedInputId;
            std::string& focusedText = g_current_vm->ui_state.inputTexts[fid];
            size_t& cur = g_current_vm->ui_state.cursorPositions[fid];
            // Clamp cursor to valid range (text may have been modified externally)
            if (cur > focusedText.size()) cur = focusedText.size();
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Left) {
                    if (cur > 0) cur--;
                } else if (keyPressed->code == sf::Keyboard::Key::Right) {
                    if (cur < focusedText.size()) cur++;
                } else if (keyPressed->code == sf::Keyboard::Key::Home) {
                    cur = 0;
                } else if (keyPressed->code == sf::Keyboard::Key::End) {
                    cur = focusedText.size();
                } else if (keyPressed->code == sf::Keyboard::Key::Delete) {
                    if (cur < focusedText.size()) {
                        focusedText.erase(cur, 1);
                        g_current_vm->ui_state.textChangedState[fid] = true;
                    }
                }
            }
            if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                if (textEntered->unicode < 128) {
                    char c = static_cast<char>(textEntered->unicode);
                    if (c == '\b') {
                        if (cur > 0) {
                            focusedText.erase(cur - 1, 1);
                            cur--;
                            g_current_vm->ui_state.textChangedState[fid] = true;
                        }
                    } else if (c >= 32 && c <= 126) {
                        focusedText.insert(cur, 1, c);
                        cur++;
                        g_current_vm->ui_state.textChangedState[g_current_vm->ui_state.focusedInputId] = true;
                    }
                }
            }
        }
    }

    g_current_vm->ui_state.clickHandlers.clear();
    int counter = 0;
    auto rootNode = build_ui_tree(static_cast<ObjInstance*>(args[0].as.obj), counter);
    g_current_vm->ui_state.rootNode = rootNode;

    auto now = std::chrono::steady_clock::now();
    float dt = 0.016f;
    if (!g_current_vm->ui_state.firstRender) {
        dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastRenderTime).count();
    }
    g_current_vm->ui_state.firstRender = false;
    g_current_vm->ui_state.lastRenderTime = now;

    apply_animations_to_tree(g_current_vm->ui_state.rootNode, dt);

    if (g_current_vm->ui_state.layoutEngineEnabled && g_current_vm->ui_state.rootNode) {
        sf::Vector2u winSize = g_current_vm->sfml_window->getSize();
        g_current_vm->ui_state.rootNode->width = (float)winSize.x;
        g_current_vm->ui_state.rootNode->height = (float)winSize.y;
        compute_sizes(g_current_vm->ui_state.rootNode);
        g_current_vm->ui_state.rootNode->width = (float)winSize.x;
        g_current_vm->ui_state.rootNode->height = (float)winSize.y;
        place_children(g_current_vm->ui_state.rootNode, 0.0f, 0.0f);
    }

    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    bool mouseJustClicked = g_current_vm->ui_state.mouseJustClicked;
    g_current_vm->ui_state.mouseJustClicked = false; // consume the click
    hit_test_tree(g_current_vm->ui_state.rootNode, m, mouseJustClicked);

    sf::Color clearColor = g_current_vm->ui_state.currentStyleColor;
    if (g_current_vm->ui_state.activeStyle != nullptr) clearColor = g_current_vm->ui_state.activeStyle->bgColor;
    g_current_vm->sfml_window->clear(clearColor);
    deferred_render_nodes.clear();
    render_ui_tree(g_current_vm->ui_state.rootNode);
    for (auto& node : deferred_render_nodes) {
        render_ui_tree(node);
    }
    g_current_vm->sfml_window->display();

    for (const auto& [id, clicked] : g_current_vm->ui_state.clickState) {
        if (clicked && g_current_vm->ui_state.clickHandlers.count(id)) {
            g_current_vm->ui_state.clickState[id] = false;
            return g_current_vm->ui_state.clickHandlers[id];
        }
    }
    
    for (const auto& [id, changed] : g_current_vm->ui_state.textChangedState) {
        if (changed && g_current_vm->ui_state.changeHandlers.count(id)) {
            g_current_vm->ui_state.textChangedState[id] = false;
            return g_current_vm->ui_state.changeHandlers[id];
        }
    }

    return {};
}

static SapphireValue native_len(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: len() expects 1 argument." << std::endl;
        }
        return {};
    }

    SapphireValue value = args[0];

    if (is_obj_type(value, OBJ_STRING)) {
        ObjString* str = static_cast<ObjString*>(value.as.obj);
        return (double)str->chars.length();
    }
    else if (is_obj_type(value, OBJ_ARRAY)) {
        auto array_obj = static_cast<ObjArray*>(value.as.obj);
        return (double)array_obj->elements.size();
    }

    if (!g_current_vm->soft_mode) {
        std::cerr << "Runtime Error: len() argument must be a string or an array." << std::endl;
    }
    return {};
}

static SapphireValue native_http_get(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: HTTP.get() expects 1 string type argument (the URL)." << std::endl;
        }
        return {};
    }

    ObjString* url_obj = static_cast<ObjString*>(args[0].as.obj);
    std::string url_str = url_obj->chars;
    std::string host, path;

    size_t host_start = url_str.find("://");
    if (host_start == std::string::npos) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Invalid URL." << std::endl;
        }
        return {};
    }
    host_start += 3;
    size_t path_start = url_str.find('/', host_start);
    if (path_start == std::string::npos) {
        host = url_str;
        path = "/";
    } else {
        host = url_str.substr(0, path_start);
        path = url_str.substr(path_start);
    }

    try {
        httplib::Client cli(host.c_str());
        cli.set_follow_location(true);
        auto res = cli.Get(path.c_str());

        if (res && res->status == 200) {
            return new_string(g_current_vm, res->body);
        }
    } catch (...) {}

    return {};
}

static SapphireValue native_http_ping(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;

    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;

    try {
        httplib::Client cli(url_str.c_str());
        cli.set_connection_timeout(std::chrono::seconds(2));
        if (auto res = cli.Get("/")) {
            return res->status == 200;
        }
    } catch (...) {}

    return false;
}

static SapphireValue native_http_post(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return {};

    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string body = static_cast<ObjString*>(args[1].as.obj)->chars;
    std::string content_type = (arg_count == 3 && is_obj_type(args[2], OBJ_STRING))
                               ? static_cast<ObjString*>(args[2].as.obj)->chars
                               : "application/json";

    try {
        httplib::Client cli(url_str.c_str());
        if (auto res = cli.Post("/", body, content_type.c_str())) {
            return new_string(g_current_vm, res->body);
        }
    } catch (...) {}

    return {};
}

static SapphireValue native_http_download(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return false;

    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string path = static_cast<ObjString*>(args[1].as.obj)->chars;

    try {
        httplib::Client cli(url_str.c_str());
        auto res = cli.Get("/");
        if (res && res->status == 200) {
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) return false;
            file << res->body;
            file.close();
            return true;
        }
    } catch (...) {}

    return false;
}

static SapphireValue native_http_serve(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_NUMBER || args[1].type != ValType::VAL_OBJ) {
        if (!g_current_vm->soft_mode) std::cerr << "Runtime Error: httpServer() expects a number and a function/closure." << std::endl;
        return false;
    }
    
    int port = (int)args[0].as.number;
    SapphireValue callback = args[1];
    
    httplib::Server svr;
    VM* original_vm = g_current_vm;
    
    auto handler = [callback, original_vm](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(thread_mutex);
        
        VM* prev_vm = g_current_vm;
        g_current_vm = original_vm;
        
        ObjMap* req_map = new_map(g_current_vm);
        req_map->items["method"] = new_string(g_current_vm, req.method);
        req_map->items["path"] = new_string(g_current_vm, req.path);
        req_map->items["body"] = new_string(g_current_vm, req.body);
        
        int target_frame_count = g_current_vm->frame_count;
        g_current_vm->push(callback);
        g_current_vm->push(SapphireValue(req_map));
        
        if (g_current_vm->call_value(callback, 1)) {
            if (g_current_vm->run(target_frame_count)) {
                SapphireValue result = g_current_vm->pop();
                if (is_obj_type(result, OBJ_STRING)) {
                    res.set_content(static_cast<ObjString*>(result.as.obj)->chars, "text/plain");
                } else if (is_obj_type(result, OBJ_MAP)) {
                    ObjMap* map_res = static_cast<ObjMap*>(result.as.obj);
                    std::string body = "";
                    std::string ctype = "text/plain";
                    if (map_res->items.count("body")) {
                        if (is_obj_type(map_res->items["body"], OBJ_STRING)) {
                            body = static_cast<ObjString*>(map_res->items["body"].as.obj)->chars;
                        }
                    }
                    if (map_res->items.count("status")) {
                        res.status = (int)map_res->items["status"].as.number;
                    }
                    if (map_res->items.count("contentType")) {
                        if (is_obj_type(map_res->items["contentType"], OBJ_STRING)) {
                            ctype = static_cast<ObjString*>(map_res->items["contentType"].as.obj)->chars;
                        }
                    }
                    
                    if (map_res->items.count("headers")) {
                        if (is_obj_type(map_res->items["headers"], OBJ_MAP)) {
                            ObjMap* headers_map = static_cast<ObjMap*>(map_res->items["headers"].as.obj);
                            for (auto const& [key, val] : headers_map->items) {
                                if (is_obj_type(val, OBJ_STRING)) {
                                    std::string header_val = static_cast<ObjString*>(val.as.obj)->chars;
                                    res.set_header(key, header_val);
                                }
                            }
                        }
                    }

                    res.set_content(body, ctype);
                }
            }
        }
        
        g_current_vm = prev_vm;
    };

    svr.Get(".*", handler);
    svr.Post(".*", handler);
    svr.Put(".*", handler);
    svr.Patch(".*", handler);
    svr.Delete(".*", handler);
    svr.Options(".*", handler);

    std::cout << "[Sapphire] HTTP Server natively listening on port " << port << "..." << std::endl;
    bool success = svr.listen("0.0.0.0", port);
    return success;
}

static SapphireValue native_json_parse(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: JSON.parse() expects 1 string argument." << std::endl;
        }
        return {};
    }

    ObjString* json_string_obj = static_cast<ObjString*>(args[0].as.obj);
    const std::string& json_string = json_string_obj->chars;

    try {
        nlohmann::json parsed_json = nlohmann::json::parse(json_string);
        return convertJsonToSapphire(g_current_vm, parsed_json);
    } catch (const std::exception& e) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Failed to parse JSON string: " << e.what() << std::endl;
        }
        return {};
    }
}

static nlohmann::json convertSapphireToJson(SapphireValue val) {
    if (val.type == ValType::VAL_NUMBER) {
        return val.as.number;
    } else if (val.type == ValType::VAL_BOOL) {
        return val.as.boolean;
    } else if (val.type == ValType::VAL_NIL) {
        return nullptr;
    } else if (is_obj_type(val, OBJ_ARRAY)) {
        auto arr = static_cast<ObjArray*>(val.as.obj);
        nlohmann::json j = nlohmann::json::array();
        for (const auto& elem : arr->elements) {
            j.push_back(convertSapphireToJson(elem));
        }
        return j;
    } else if (val.type == ValType::VAL_OBJ) {
        Obj* obj = val.as.obj;
        if (obj->type == OBJ_STRING) {
            return static_cast<ObjString*>(obj)->chars;
        } else if (obj->type == OBJ_MAP) {
            ObjMap* map = static_cast<ObjMap*>(obj);
            nlohmann::json j = nlohmann::json::object();
            for (const auto& pair : map->items) {
                j[pair.first] = convertSapphireToJson(pair.second);
            }
            return j;
        } else if (obj->type == OBJ_INSTANCE) {
            ObjInstance* instance = static_cast<ObjInstance*>(obj);
            nlohmann::json j = nlohmann::json::object();
            for (const auto& pair : instance->fields) {
                j[pair.first] = convertSapphireToJson(pair.second);
            }
            return j;
        }
    }
    return nullptr;
}

static SapphireValue native_json_stringify(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: JSON.stringify() expects 1 argument." << std::endl;
        }
        return {};
    }
    nlohmann::json j = convertSapphireToJson(args[0]);
    return new_string(g_current_vm, j.dump());
}

static SapphireValue core_create_instance(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Core.createInstance() expects 1 string argument (class name)." << std::endl;
        }
        return {};
    }

    ObjString* class_name_obj = static_cast<ObjString*>(args[0].as.obj);
    std::string class_name = class_name_obj->chars;

    auto it = g_current_vm->globals.find(class_name);
    if (it == g_current_vm->globals.end()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Class '" << class_name << "' not found." << std::endl;
        }
        return {};
    }

    SapphireValue class_value = it->second;

    if (class_value.type != ValType::VAL_OBJ || class_value.as.obj->type != OBJ_CLASS) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Global variable '" << class_name << "' is not a class." << std::endl;
        }
        return {};
    }

    ObjClass* klass = static_cast<ObjClass*>(class_value.as.obj);
    ObjInstance* instance = new_instance(g_current_vm, klass);
    return instance;
}

static SapphireValue native_list_util_create(int arg_count, SapphireValue* args) {
    if (arg_count != 0) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.create() expects 0 arguments." << std::endl;
        }
        return {};
    }
    return new_array(g_current_vm);
}

static SapphireValue native_list_util_append(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_ARRAY)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.append() expects a list and a value." << std::endl;
        }
        return {};
    }
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    list_obj->elements.push_back(args[1]);
    return args[0];
}

static SapphireValue native_list_util_get(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_ARRAY) || args[1].type != ValType::VAL_NUMBER) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.get() expects a list and an index (number)." << std::endl;
        }
        return {};
    }
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    int index = static_cast<int>(args[1].as.number);

    if (index < 0 || index >= list_obj->elements.size()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.get(): Index out of bounds." << std::endl;
        }
        return {};
    }
    return list_obj->elements[index];
}

static SapphireValue native_list_util_set(int arg_count, SapphireValue* args) {
    if (arg_count != 3 || !is_obj_type(args[0], OBJ_ARRAY) || args[1].type != ValType::VAL_NUMBER) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.set() expects a list, an index (number), and a value." << std::endl;
        }
        return {};
    }
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    int index = static_cast<int>(args[1].as.number);
    SapphireValue new_value = args[2];

    if (index < 0 || index >= list_obj->elements.size()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.set(): Index out of bounds." << std::endl;
        }
        return {};
    }
    list_obj->elements[index] = new_value;
    return args[0];
}

static SapphireValue native_list_util_length(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_ARRAY)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.length() expects 1 list argument." << std::endl;
        }
        return 0.0;
    }
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    return (double)list_obj->elements.size();
}

static SapphireValue native_list_util_remove_at(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_ARRAY) || args[1].type != ValType::VAL_NUMBER) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.removeAt() expects a list and an index (number)." << std::endl;
        }
        return {};
    }
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    int index = static_cast<int>(args[1].as.number);

    if (index < 0 || index >= list_obj->elements.size()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.removeAt(): Index out of bounds." << std::endl;
        }
        return {};
    }

    SapphireValue removed_value = list_obj->elements[index];
    list_obj->elements.erase(list_obj->elements.begin() + index);
    return removed_value;
}

static SapphireValue native_list_util_contains(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_ARRAY)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.contains() expects a list and a value." << std::endl;
        }
        return false;
    }
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    SapphireValue value_to_find = args[1];

    for (const auto& element : list_obj->elements) {
        if (values_equal(element, value_to_find)) {
            return true;
        }
    }
    return false;
}

static SapphireValue native_math_rand(int arg_count, SapphireValue* args) {
    if (arg_count > 2) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Math.rand() expects 0, 1 or 2 arguments." << std::endl;
        }
        return {};
    }

    double min_val = 0.0;
    double max_val = 1.0;

    if (arg_count == 1) {
        if (args[0].type != ValType::VAL_NUMBER) {
            if (!g_current_vm->soft_mode) {
                std::cerr << "Runtime Error: Math.rand(max) expects a number for max value." << std::endl;
            }
            return {};
        }
        max_val = args[0].as.number;
    } else if (arg_count == 2) {
        if (args[0].type != ValType::VAL_NUMBER || args[1].type != ValType::VAL_NUMBER) {
            if (!g_current_vm->soft_mode) {
                std::cerr << "Runtime Error: Math.rand(min, max) expects numbers for min and max values." << std::endl;
            }
            return {};
        }
        min_val = args[0].as.number;
        max_val = args[1].as.number;
    }

    if (min_val > max_val) {
        std::swap(min_val, max_val);
    }

    std::uniform_real_distribution<double> distrib(min_val, max_val);
    return distrib(gen);
}

static SapphireValue native_string_to_double(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return 0.0;
    }
    ObjString* str = static_cast<ObjString*>(args[0].as.obj);
    try {
        return std::stod(str->chars);
    } catch (const std::exception&) {
        return 0.0;
    }
}

static SapphireValue native_evaluate(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return new_string(g_current_vm, "Error");
    }

    ObjString* source_string = static_cast<ObjString*>(args[0].as.obj);
    std::string source_to_run = source_string->chars;

    VM* previous_vm = g_current_vm;

    ScriptConfig temp_config;
    VM temp_vm;
    temp_vm.globals = previous_vm->globals;

    g_current_vm = &temp_vm;
    SapphireValue result = temp_vm.interpret(source_to_run);
    g_current_vm = previous_vm;

    if (result.type == ValType::VAL_NIL) {
        return new_string(g_current_vm, "Error");
    }
    else if (result.type == ValType::VAL_BOOL) {
        return new_string(g_current_vm, result.as.boolean ? "true" : "false");
    }
    else if (result.type == ValType::VAL_NUMBER) {
        double num = result.as.number;
        std::string s = std::to_string(num);
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.') {
            s.pop_back();
        }
        return new_string(g_current_vm, s);
    }
    else if (is_obj_type(result, OBJ_STRING)) {
        ObjString* str_obj = static_cast<ObjString*>(result.as.obj);
        return new_string(g_current_vm, str_obj->chars);
    }

    return new_string(g_current_vm, "Error");
}



static SapphireValue native_system_sleep(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) {
        return {};
    }
    int ms = static_cast<int>(args[0].as.number);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return {};
}

static SapphireValue native_system_exec(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: exec() expects 1 string argument (the command)." << std::endl;
        }
        return {};
    }
    std::string command = static_cast<ObjString*>(args[0].as.obj)->chars;
    int result = std::system(command.c_str());
    return (double)result;
}

static SapphireValue native_system_get_os(int arg_count, SapphireValue* args) {
#ifdef _WIN32
    return new_string(g_current_vm, "Windows");
#elif __APPLE__
    return new_string(g_current_vm, "MacOS");
#elif __linux__
    return new_string(g_current_vm, "Linux");
#else
    return new_string(g_current_vm, "Unknown");
#endif
}

static SapphireValue native_system_get_env(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return {};
    }

    std::string var_name = static_cast<ObjString*>(args[0].as.obj)->chars;
    const char* env_value = std::getenv(var_name.c_str());

    if (env_value) {
        return new_string(g_current_vm, env_value);
    }

    if (arg_count == 2) {
        return args[1];
    }

    return {};
}

static SapphireValue native_system_get_clipboard(int arg_count, SapphireValue* args) {
    std::string text = sf::Clipboard::getString().toAnsiString();
    return new_string(g_current_vm, text);
}

static SapphireValue native_debug_print_stack(int arg_count, SapphireValue* args) {
    std::cout << "--- SAPPHIRE STACK DUMP ---" << std::endl;
    for (SapphireValue* slot = g_current_vm->stack; slot < g_current_vm->stack_top; slot++) {
        std::cout << "[ ";
        print_value(*slot);
        std::cout << " ]" << std::endl;
    }
    std::cout << "--- END OF STACK ---" << std::endl;
    return {};
}

static SapphireValue native_debug_dump_globals(int arg_count, SapphireValue* args) {
    std::cout << "--- SAPPHIRE GLOBALS DUMP ---" << std::endl;
    for (auto const& [name, value] : g_current_vm->globals) {
        std::cout << name << " => ";
        print_value(value);
        std::cout << std::endl;
    }
    return {};
}

static SapphireValue native_math_abs(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::abs(args[0].as.number);
}

static SapphireValue native_math_floor(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::floor(args[0].as.number);
}

static SapphireValue native_math_ceil(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::ceil(args[0].as.number);
}

static SapphireValue native_math_sin(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::sin(args[0].as.number);
}

static SapphireValue native_math_cos(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::cos(args[0].as.number);
}

static SapphireValue native_math_log(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::log(args[0].as.number);
}

static SapphireValue native_get_quote(int arg_count, SapphireValue* args) {
    return new_string(g_current_vm, "\"");
}

static SapphireValue native_math_pow(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return 0.0;
    return std::pow(args[0].as.number, args[1].as.number);
}

static SapphireValue native_math_min(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return args[0];
    double a = args[0].as.number;
    double b = args[1].as.number;
    return std::min(a, b);
}

static SapphireValue native_math_max(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return args[0];
    double a = args[0].as.number;
    double b = args[1].as.number;
    return std::max(a, b);
}

static SapphireValue native_math_clamp(int arg_count, SapphireValue* args) {
    if (arg_count < 3) return args[0];
    double v = args[0].as.number;
    double lo = args[1].as.number;
    double hi = args[2].as.number;
    return std::clamp(v, lo, hi);
}

static SapphireValue native_math_lerp(int arg_count, SapphireValue* args) {
    if (arg_count < 3) return args[0];
    double a = args[0].as.number;
    double b = args[1].as.number;
    double t = args[2].as.number;
    return a + t * (b - a);
}

static SapphireValue native_color_hex_to_rgb(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return {};

    std::string hex = static_cast<ObjString*>(args[0].as.obj)->chars;
    if (hex[0] == '#') hex.erase(0, 1);
    if (hex.length() != 6) return {};

    uint32_t value = std::stoul(hex, nullptr, 16);
    auto array_obj = new_array(g_current_vm);
    array_obj->elements.push_back((double)((value >> 16) & 0xFF)); // R
    array_obj->elements.push_back((double)((value >> 8) & 0xFF));  // G
    array_obj->elements.push_back((double)(value & 0xFF));         // B

    return array_obj;
}

static SapphireValue native_check_collision(int arg_count, SapphireValue* args) {
    if (arg_count < 8) return false;
    double x1 = args[0].as.number;
    double y1 = args[1].as.number;
    double w1 = args[2].as.number;
    double h1 = args[3].as.number;
    double x2 = args[4].as.number;
    double y2 = args[5].as.number;
    double w2 = args[6].as.number;
    double h2 = args[7].as.number;

    return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

VM::VM() : VM(ScriptConfig{}) {
}

VM::VM(const ScriptConfig& config) : VM(config, false, nullptr) {
}

VM::VM(const ScriptConfig& config, bool init_ui, sf::RenderWindow* window) : config(config) {
    g_current_vm = this;
    this->frame_count = 0;
    this->stack_top = stack;
    this->objects = nullptr;
    this->sfml_window = window;

    catch_count = 0;
  
    define_native("clock", clock_native);
    define_native("assert", assert_native);
    define_native("parseDouble", native_string_to_double);
    define_native("valueToString", native_value_to_string);
    define_native("lruCreate", native_lru_create);
    define_native("lruHas", native_lru_has);
    define_native("lruGet", native_lru_get);
    define_native("lruPut", native_lru_put);
    define_native("evaluate", native_evaluate);
    define_native("len", native_len);
    define_native("stringCharAt", native_string_char_at);
    define_native("stringLength", native_string_length);
    define_native("stringSubstring", native_string_substring);
    define_native("stringSplit", native_string_split);
    define_native("stringReplace", native_string_replace);
    define_native("stringToUpper", native_string_to_upper);
    define_native("stringToLower", native_string_to_lower);
    define_native("stringTrim", native_string_trim);
    define_native("stringContains", native_string_contains);
    define_native("getQuote", native_get_quote);

    const char* appdata_path = getenv("APPDATA");
    if (appdata_path) {
        std::string global_plugins_path = std::string(appdata_path) + "\\Sapphire\\plugins";
        module_search_paths.push_back(global_plugins_path);
    }

    // --- IO ---
    define_native("readLine", io_readline_native);
    define_native("printColor", native_io_print_color);
    define_native("readInput", native_io_read_input);
    define_native("writeFile", native_io_write_file);
    define_native("readFile", native_io_read_file);
    define_native("exists", native_io_exists);
    define_native("deleteFile", native_io_delete_file);
    define_native("appendFile", native_io_append_file);
    define_native("openFileDialog", native_io_open_file_dialog);

    // --- Math ---
    define_native("sqrt", native_math_sqrt);
    define_native("rand", native_math_rand);
    define_native("abs", native_math_abs);
    define_native("floor", native_math_floor);
    define_native("ceil", native_math_ceil);
    define_native("sin", native_math_sin);
    define_native("cos", native_math_cos);
    define_native("log", native_math_log);
    define_native("pow", native_math_pow);
    define_native("min", native_math_min);
    define_native("max", native_math_max);
    define_native("clamp", native_math_clamp);
    define_native("lerp", native_math_lerp);

    // --- JSON ---
    ObjString* json_name = new_string(this, "JSON");
    ObjClass* json_class = new_class(this, json_name);
    json_class->methods["parse"] = SapphireValue(new_native(this, native_json_parse));
    json_class->methods["stringify"] = SapphireValue(new_native(this, native_json_stringify));
    globals["JSON"] = SapphireValue(json_class);

    // --- Core ---
    define_native("createInstance", core_create_instance);

    // --- ListUtil ---
    define_native("listCreate", native_list_util_create);
    define_native("listAppend", native_list_util_append);
    define_native("listGet", native_list_util_get);
    define_native("listSet", native_list_util_set);
    define_native("listLength", native_list_util_length);
    define_native("listRemoveAt", native_list_util_remove_at);
    define_native("listContains", native_list_util_contains);

    // --- System ---
    define_native("getEnv", native_system_get_env);
    define_native("getOS", native_system_get_os);
    define_native("sleep", native_system_sleep);
    define_native("getClipboard", native_system_get_clipboard);
    define_native("exec", native_system_exec);
    define_native("spawn", native_spawn);
    define_native("join", native_join);
    define_native("getCoreCount", native_system_core_count);

    // --- Threading / Mutex ---
    ObjString* mutex_name = new_string(this, "Mutex");
    ObjClass* mutex_class = new_class(this, mutex_name);
    mutex_class->methods["new"] = SapphireValue(new_native(this, native_mutex_new));
    mutex_class->methods["lock"] = SapphireValue(new_native(this, native_mutex_lock));
    mutex_class->methods["unlock"] = SapphireValue(new_native(this, native_mutex_unlock));
    globals["Mutex"] = SapphireValue(mutex_class);

    // --- OpenCL ---
    define_opencl_natives(this);

    // --- HTTP ---
    define_native("httpGet", native_http_get);
    define_native("httpPost", native_http_post);
    define_native("httpPing", native_http_ping);
    define_native("httpDownload", native_http_download);
    define_native("httpServer", native_http_serve);

    // --- Color ---
    define_native("hexToRGB", native_color_hex_to_rgb);

    // --- Debug ---
    define_native("printStack", native_debug_print_stack);
    define_native("dumpGlobals", native_debug_dump_globals);

    // --- checkCollision ---
    define_native("checkCollision", native_check_collision);

    // Registro SFML puro
    register_graphics_engine(this);
    register_vec2d_class(this);
    register_vec3d_class(this);
    define_sqlite_natives(this);

    if (init_ui) {
        g_current_vm = this;

        std::vector<std::string> fontsToLoad = { "ARIAL.TTF", "Courier.ttf", "TimesNewRoman.ttf" };
        std::vector<std::string> aliases = { "Arial", "Courier", "TimesNewRoman" };

        for (size_t i = 0; i < fontsToLoad.size(); i++) {
            sf::Font font;
            std::string path = "data/fonts/" + fontsToLoad[i];
            if (font.openFromFile(path)) {
                this->ui_state.fontStack[aliases[i]] = font;
                if (i == 0) this->sapphire_font = font;
            }
        }

        if (this->ui_state.fontStack.find("default") == this->ui_state.fontStack.end()) {
            this->ui_state.fontStack["default"] = this->sapphire_font;
        }

        this->ui_component_class = new_class(this, new_string(this, "UIComponent"));
        define_native("Render", native_ui_render);
        define_native("Style", native_ui_style);
        define_native("Flex", native_ui_flex);
        define_native("Button", native_ui_button);
        define_native("Text", native_ui_text);
        define_native("Display", native_ui_display);
        define_native("Checkbox", native_ui_checkbox);
        define_native("Slider", native_ui_slider);
        define_native("Input", native_ui_input);
        define_native("Separator", native_ui_separator);
        define_native("GetInputText", native_ui_get_input_text);
        define_native("Menu", native_ui_menu);
        define_native("MenuItem", native_ui_menuitem);
        
        // Advanced & Layouts
        define_native("Grid", native_ui_grid);
        define_native("StackPanel", native_ui_stackpanel);
        define_native("DockPanel", native_ui_dockpanel);
        define_native("WrapPanel", native_ui_wrappanel);
        define_native("ScrollView", native_ui_scrollview);
        define_native("Border", native_ui_border);
        
        // Controls
        define_native("Image", native_ui_image);
        define_native("ProgressBar", native_ui_progressbar);
        define_native("RadioBox", native_ui_radiobox);
        define_native("ToggleSwitch", native_ui_toggleswitch);
        define_native("ComboBox", native_ui_combobox);
        define_native("ListBox", native_ui_listbox);
        define_native("PasswordBox", native_ui_passwordbox);
        define_native("Hyperlink", native_ui_hyperlink);
        define_native("Expander", native_ui_expander);
        
        // Specialized
        define_native("DataGrid", native_ui_datagrid);
        define_native("Canvas", native_ui_canvas);
        define_native("Tooltip", native_ui_tooltip);
        define_native("Popup", native_ui_popup);
        define_native("Window", native_ui_window);
        
        // Animations
        define_native("Animate", native_ui_animate);

        globals["_ui_initialized"] = {};
        globals["APP_WINDOW_WIDTH"] = (double)config.windowWidth;
        globals["APP_WINDOW_HEIGHT"] = (double)config.windowHeight;
    }
}
void VM::add_module_search_path(const std::string& path) {
    module_search_paths.push_back(path);
}

static std::string get_custom_entry_point(const std::string& base_dir) {
    // 1. Check PLUGIN.txt
    std::string plugin_txt_path = base_dir + "/PLUGIN.txt";
    std::ifstream infile(plugin_txt_path);
    if (infile.good()) {
        std::string line;
        while (std::getline(infile, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            std::string trimmed = line.substr(start);
            if (trimmed.rfind("main:", 0) == 0) {
                std::string val = trimmed.substr(5);
                size_t vstart = val.find_first_not_of(" \t\r\n");
                if (vstart != std::string::npos) {
                    std::string entry = val.substr(vstart);
                    while (!entry.empty() && (entry.back() == '\r' || entry.back() == '\n' || entry.back() == ' ')) {
                        entry.pop_back();
                    }
                    infile.close();
                    return entry;
                }
            }
            if (trimmed.rfind("entry:", 0) == 0) {
                std::string val = trimmed.substr(6);
                size_t vstart = val.find_first_not_of(" \t\r\n");
                if (vstart != std::string::npos) {
                    std::string entry = val.substr(vstart);
                    while (!entry.empty() && (entry.back() == '\r' || entry.back() == '\n' || entry.back() == ' ')) {
                        entry.pop_back();
                    }
                    infile.close();
                    return entry;
                }
            }
        }
    }
    infile.close();

    // 2. Check sapphire.json
    std::string json_path = base_dir + "/sapphire.json";
    std::ifstream json_file(json_path);
    if (json_file.good()) {
        try {
            nlohmann::json j;
            json_file >> j;
            json_file.close();
            if (j.contains("main") && j["main"].is_string()) {
                return j["main"].get<std::string>();
            }
            if (j.contains("entry") && j["entry"].is_string()) {
                return j["entry"].get<std::string>();
            }
        } catch(...) {}
    }
    json_file.close();

    // Default entry point
    return "files/main.sp";
}

std::string VM::find_and_load_module(const std::string& module_name, std::string& out_resolved_path) {
    std::string target_name = module_name;
    
    // Check for explicit import scope prefix
    bool force_local = false;
    bool force_global = false;
    bool direct_path = false;
    std::string explicit_path = "";
    
    if (target_name.rfind("local:", 0) == 0) {
        force_local = true;
        target_name = target_name.substr(6);
    } else if (target_name.rfind("global:", 0) == 0) {
        force_global = true;
        target_name = target_name.substr(7);
    } else if (target_name.rfind("path:", 0) == 0) {
        direct_path = true;
        explicit_path = target_name.substr(5);
    }

    if (direct_path) {
        std::string entry = get_custom_entry_point(explicit_path);
        std::string full_path = explicit_path + "/" + entry;
        std::string content = load_file_as_string(full_path);
        if (!content.empty()) {
            try {
                out_resolved_path = std::filesystem::absolute(full_path).string();
            } catch(...) {
                out_resolved_path = full_path;
            }
            return content;
        }
        return "";
    }

    // Check if this is a plugin import (format: plugin@version or plugin@latest)
    size_t at_pos = target_name.find('@');
    if (at_pos != std::string::npos || force_local || force_global) {
        std::string plugin_name = target_name;
        std::string version = "latest";
        if (at_pos != std::string::npos) {
            plugin_name = target_name.substr(0, at_pos);
            version = target_name.substr(at_pos + 1);
        }
        
        // Define base paths to try based on options
        std::vector<std::string> base_dirs;
        
        const char* appdata_path = getenv("APPDATA");
        
        // If not forced local, global (APPDATA) is preferred search path
        if (!force_local) {
            if (appdata_path != nullptr) {
                base_dirs.push_back(
                    std::string(appdata_path) + "\\Sapphire\\plugins\\" + plugin_name +
                    "\\versions\\v" + version);
            }
        }
        
        // If not forced global, check local project-level paths
        if (!force_global) {
            base_dirs.push_back("plugins/" + plugin_name + "/versions/v" + version);
            base_dirs.push_back("../plugins/" + plugin_name + "/versions/v" + version);
            base_dirs.push_back("../../plugins/" + plugin_name + "/versions/v" + version);
            base_dirs.push_back("../" + plugin_name + "/versions/v" + version);
            base_dirs.push_back(plugin_name + "/versions/v" + version);
            if (plugin_name == "vividry" || plugin_name == "Vividry") {
                base_dirs.push_back("Vividry/versions/v" + version);
            }
        }
        
        // Try to load
        for (const auto& base_dir : base_dirs) {
            std::string entry = get_custom_entry_point(base_dir);
            std::string full_path = base_dir + "/" + entry;
            std::string content = load_file_as_string(full_path);
            if (!content.empty()) {
                try {
                    out_resolved_path = std::filesystem::absolute(full_path).string();
                } catch(...) {
                    out_resolved_path = full_path;
                }
                return content;
            }
        }
        
        // Fallback for "latest" version if not found
        if (version == "latest") {
            std::vector<std::string> latest_base_dirs;
            if (!force_local) {
                if (appdata_path != nullptr) {
                    latest_base_dirs.push_back(
                        std::string(appdata_path) + "\\Sapphire\\plugins\\" + plugin_name +
                        "\\versions\\v1.0.0");
                }
            }
            if (!force_global) {
                latest_base_dirs.push_back("plugins/" + plugin_name + "/versions/v1.0.0");
                latest_base_dirs.push_back("../plugins/" + plugin_name + "/versions/v1.0.0");
                latest_base_dirs.push_back("../../plugins/" + plugin_name + "/versions/v1.0.0");
                latest_base_dirs.push_back("../" + plugin_name + "/versions/v1.0.0");
                latest_base_dirs.push_back(plugin_name + "/versions/v1.0.0");
                if (plugin_name == "vividry" || plugin_name == "Vividry") {
                    latest_base_dirs.push_back("Vividry/versions/v1.0.0");
                }
            }
            
            for (const auto& base_dir : latest_base_dirs) {
                std::string entry = get_custom_entry_point(base_dir);
                std::string full_path = base_dir + "/" + entry;
                std::string content = load_file_as_string(full_path);
                if (!content.empty()) {
                    try {
                        out_resolved_path = std::filesystem::absolute(full_path).string();
                    } catch(...) {
                        out_resolved_path = full_path;
                    }
                    return content;
                }
            }
        }
        
        return "";
    }
    
    // Traditional file path import
    std::string content = load_file_as_string(module_name);
    if (!content.empty()) {
        try {
            out_resolved_path = std::filesystem::absolute(module_name).string();
        } catch(...) {
            out_resolved_path = module_name;
        }
        return content;
    }

    for (const std::string& base_path : module_search_paths) {
        std::string path_direct = base_path + "\\" + module_name;
        content = load_file_as_string(path_direct);
        if (!content.empty()) {
            try { out_resolved_path = std::filesystem::absolute(path_direct).string(); } catch(...) { out_resolved_path = path_direct; }
            return content;
        }

        std::string path_sp = base_path + "\\" + module_name + ".sp";
        content = load_file_as_string(path_sp);
        if (!content.empty()) {
            try { out_resolved_path = std::filesystem::absolute(path_sp).string(); } catch(...) { out_resolved_path = path_sp; }
            return content;
        }

        std::string path_main_sp = base_path + "\\" + module_name + "\\main.sp";
        content = load_file_as_string(path_main_sp);
        if (!content.empty()) {
            try { out_resolved_path = std::filesystem::absolute(path_main_sp).string(); } catch(...) { out_resolved_path = path_main_sp; }
            return content;
        }
    }

    out_resolved_path = "";
    return "";
}

VM::~VM() {
    Obj* object = objects;
    while (object != nullptr) {
        Obj* next = object->next;
        free_object(object);
        object = next;
    }
    if (g_current_vm == this) {
        g_current_vm = nullptr;
    }
}

void VM::setGlobalNumber(const std::string& name, double value) {
    globals[name] = value;
}

void VM::define_native(const std::string& name, NativeFn function) {
    ObjNative* native = new_native(this, function);
    native->name = new_string(this, name);

    globals[name] = native;
}
void VM::push(const SapphireValue& value) {
    *stack_top = value;
    stack_top++;
}
SapphireValue VM::pop() {
    if (stack_top == stack) {
        std::cerr << "Runtime Error: Stack underflow." << std::endl;
        exit(70);
    }
    stack_top--;

    return *stack_top;
}
SapphireValue& VM::peek(int distance) {
    return stack_top[-1 - distance];
}

ObjFunction* VM::compile_module(const std::string& source) {
    return compile(this, source);
}

bool VM::call(ObjFunction* function, int arg_count) {
    if (function == nullptr) return false;

    if (arg_count != function->arity) {
        arg_count = function->arity;
    }

    if (function->is_async) {
        ObjPromise* promise = new_promise(this);
        promise->function = function;
        for (int i = 0; i < arg_count; i++) {
            promise->args.push_back(stack_top[-arg_count + i]);
        }
        stack_top -= arg_count + 1; // pop args and function
        push(SapphireValue((Obj*)promise));
        event_loop_queue.push_back(promise);
        return true;
    }

    if (frame_count == FRAMES_MAX) {
        if (!this->soft_mode) std::cerr << "Runtime Error: Stack overflow." << std::endl;
        return false;
    }

    CallFrame* frame = &frames[frame_count++];
    frame->function = function;
    frame->ip = &function->chunk.code[0];

    frame->slots = stack_top - arg_count - 1;
    return true;
}

bool VM::call_value(SapphireValue callee, int arg_count) {
    bool is_callable = callee.type == ValType::VAL_OBJ &&
                      (callee.as.obj->type == OBJ_CLOSURE ||
                       callee.as.obj->type == OBJ_NATIVE ||
                       callee.as.obj->type == OBJ_CLASS ||
                       callee.as.obj->type == OBJ_BOUND_METHOD ||
                       callee.as.obj->type == OBJ_FUNCTION);

    if (!is_callable) {
        for (int i = 1; i <= 4; i++) {
            SapphireValue potential = stack_top[-arg_count - 1 - i];
            if (potential.type == ValType::VAL_OBJ) {
                Obj* o = potential.as.obj;
                if (o->type == OBJ_CLOSURE || o->type == OBJ_NATIVE || o->type == OBJ_CLASS || o->type == OBJ_BOUND_METHOD || o->type == OBJ_FUNCTION) {
                    callee = potential;
                    stack_top[-arg_count - 1] = callee;
                    is_callable = true;
                    break;
                }
            }
        }
    }

    if (is_callable) {
        Obj* obj = callee.as.obj;
        switch (obj->type) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = (ObjBoundMethod*)obj;
                stack_top[-arg_count - 1] = bound->receiver;
                return call_value(bound->method, arg_count);
            }
            case OBJ_CLASS: {
                if (arg_count != 0) {
                    if (!this->soft_mode) std::cerr << "Runtime Error: Expected 0 arguments but got " << arg_count << "." << std::endl;
                    return false;
                }
                ObjClass* klass = (ObjClass*)obj;
                stack_top[-arg_count - 1] = new_instance(this, klass);
                return true;
            }
            case OBJ_CLOSURE:
                return call(((ObjClosure*)obj)->function, arg_count);
            case OBJ_FUNCTION:
                return call((ObjFunction*)obj, arg_count);
            case OBJ_NATIVE: {
                NativeFn native = ((ObjNative*)obj)->function;
                SapphireValue result = native(arg_count, stack_top - arg_count);
                stack_top -= arg_count + 1;
                push(result);
                return true;
            }
            default: break;
        }
    }

    if (!this->soft_mode) {
        std::cerr << "Runtime Error: Can only call functions and classes." << std::endl;
        try {
            std::cerr << "  Callee type: " << get_value_type_name(callee) << "  Value: ";
            print_value(callee);
            std::cerr << std::endl;
        } catch (...) {}

        // Dump a small window of the stack around the call site for diagnosis
        std::cerr << "  Stack (top-most last):\n";
        int max_dump = 12;
        int available = static_cast<int>(stack_top - stack);
        int start = std::max(0, available - max_dump);
        for (int i = start; i < available; ++i) {
            std::cerr << "    [" << i << "] ";
            try { print_value(stack[i]); } catch (...) { std::cerr << "<err>"; }
            std::cerr << std::endl;
        }
    }
    return false;
}

        // std::cout << "    [CALL_VALUE SPY] Despachando chamada para objeto tipo: " << obj->type << std::endl;
        // Eu nem tiro mais os debugs, vai que eu preciso Â¯\_(ãƒ„)_/Â¯

bool VM::run(int target_frame_count) {
    CallFrame* frame = &frames[frame_count - 1];
    uint8_t* ip = frame->ip;
    SapphireValue* slots = frame->slots;
    #define top stack_top

#ifndef _MSC_VER
    static const void* dispatch_table[255];
    static bool table_initialized = false;
#endif

    // VariÃ¡veis auxiliares declaradas fora para evitar erro de inicializaÃ§Ã£o cruzada
    ObjString* name_tmp;
    std::string src_tmp;
    std::string resolved_path_tmp;
    ObjFunction* func_tmp;
    int timer_counter = 0;
    SapphireValue val_tmp;

    auto evaluate_fade = [](SapphireValue val) -> SapphireValue {
        if (val.type == ValType::VAL_OBJ && val.as.obj->type == OBJ_FADE) {
            ObjFade* fade = (ObjFade*)val.as.obj;
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
            uint64_t elapsed = ms - fade->created_at_ms;
            
            if (elapsed >= fade->duration_ms) {
                return SapphireValue(); // NIL (faded)
            }
            
            if (fade->value.type == ValType::VAL_NUMBER) {
                double progress = (double)elapsed / fade->duration_ms;
                double multiplier = 1.0;
                if (fade->curve_type == "linear") multiplier = 1.0 - progress;
                else if (fade->curve_type == "exponential") multiplier = std::exp(-5.0 * progress);
                return SapphireValue(fade->value.as.number * multiplier);
            }
            return fade->value;
        }
        return val;
    };

#ifndef _MSC_VER
    if (!table_initialized) {
        for (int i = 0; i < 255; i++) dispatch_table[i] = &&op_OP_UNKNOWN;
        dispatch_table[OP_CONSTANT] = &&op_OP_CONSTANT;
        dispatch_table[OP_NIL] = &&op_OP_NIL;
        dispatch_table[OP_TRUE] = &&op_OP_TRUE;
        dispatch_table[OP_FALSE] = &&op_OP_FALSE;
        dispatch_table[OP_POP] = &&op_OP_POP;
        dispatch_table[OP_GET_LOCAL] = &&op_OP_GET_LOCAL;
        dispatch_table[OP_SET_LOCAL] = &&op_OP_SET_LOCAL;
        dispatch_table[OP_GET_GLOBAL] = &&op_OP_GET_GLOBAL;
        dispatch_table[OP_DEFINE_GLOBAL] = &&op_OP_DEFINE_GLOBAL;
        dispatch_table[OP_SET_GLOBAL] = &&op_OP_SET_GLOBAL;
        dispatch_table[OP_GET_PROPERTY] = &&op_OP_GET_PROPERTY;
        dispatch_table[OP_SET_PROPERTY] = &&op_OP_SET_PROPERTY;
        dispatch_table[OP_EQUAL] = &&op_OP_EQUAL;
        dispatch_table[OP_GREATER] = &&op_OP_GREATER;
        dispatch_table[OP_LESS] = &&op_OP_LESS;
        dispatch_table[OP_ADD] = &&op_OP_ADD;
        dispatch_table[OP_SUBTRACT] = &&op_OP_SUBTRACT;
        dispatch_table[OP_MULTIPLY] = &&op_OP_MULTIPLY;
        dispatch_table[OP_DIVIDE] = &&op_OP_DIVIDE;
        dispatch_table[OP_MODULO] = &&op_OP_MODULO;
        dispatch_table[OP_BITWISE_AND] = &&op_OP_BITWISE_AND;
        dispatch_table[OP_BITWISE_OR] = &&op_OP_BITWISE_OR;
        dispatch_table[OP_BITWISE_XOR] = &&op_OP_BITWISE_XOR;
        dispatch_table[OP_BITWISE_NOT] = &&op_OP_BITWISE_NOT;
        dispatch_table[OP_LEFT_SHIFT] = &&op_OP_LEFT_SHIFT;
        dispatch_table[OP_RIGHT_SHIFT] = &&op_OP_RIGHT_SHIFT;
        dispatch_table[OP_NOT] = &&op_OP_NOT;
        dispatch_table[OP_NEGATE] = &&op_OP_NEGATE;
        dispatch_table[OP_PRINT] = &&op_OP_PRINT;
        dispatch_table[OP_JUMP] = &&op_OP_JUMP;
        dispatch_table[OP_JUMP_IF_FALSE] = &&op_OP_JUMP_IF_FALSE;
        dispatch_table[OP_JUMP_IF_NIL] = &&op_OP_JUMP_IF_NIL;
        dispatch_table[OP_JUMP_IF_NOT_NIL] = &&op_OP_JUMP_IF_NOT_NIL;
        dispatch_table[OP_LOOP] = &&op_OP_LOOP;
        dispatch_table[OP_CALL] = &&op_OP_CALL;
        dispatch_table[OP_CLOSURE] = &&op_OP_CLOSURE;
        dispatch_table[OP_RETURN] = &&op_OP_RETURN;
        dispatch_table[OP_BUILD_ARRAY] = &&op_OP_BUILD_ARRAY;
        dispatch_table[OP_BUILD_MAP] = &&op_OP_BUILD_MAP;
        dispatch_table[OP_GET_SUBSCRIPT] = &&op_OP_GET_SUBSCRIPT;
        dispatch_table[OP_SET_SUBSCRIPT] = &&op_OP_SET_SUBSCRIPT;
        dispatch_table[OP_SPREAD_ARRAY] = &&op_OP_SPREAD_ARRAY;
        dispatch_table[OP_IMPORT] = &&op_OP_IMPORT;
        dispatch_table[OP_MAKE_NAMED_ARG] = &&op_OP_MAKE_NAMED_ARG;
        dispatch_table[OP_DUP] = &&op_OP_DUP;
        dispatch_table[OP_TRY_START] = &&op_OP_TRY_START;
        dispatch_table[OP_TRY_END] = &&op_OP_TRY_END;
        dispatch_table[OP_THROW] = &&op_OP_THROW;
        dispatch_table[OP_INHERIT] = &&op_OP_INHERIT;
        dispatch_table[OP_GET_SUPER] = &&op_OP_GET_SUPER;
        dispatch_table[OP_SPAWN] = &&op_OP_SPAWN;
        dispatch_table[OP_AWAIT] = &&op_OP_AWAIT;
        dispatch_table[OP_ASYNC_CALL] = &&op_OP_ASYNC_CALL;
        dispatch_table[OP_GET_ITERATOR] = &&op_OP_GET_ITERATOR;
        dispatch_table[OP_ITER_NEXT_IN] = &&op_OP_ITER_NEXT_IN;
        dispatch_table[OP_ITER_NEXT_OF] = &&op_OP_ITER_NEXT_OF;
        dispatch_table[OP_WITHIN_START] = &&op_OP_WITHIN_START;
        dispatch_table[OP_WITHIN_END] = &&op_OP_WITHIN_END;
        dispatch_table[OP_EVERY_TICK] = &&op_OP_EVERY_TICK;
        dispatch_table[OP_UNDO] = &&op_OP_UNDO;
        dispatch_table[OP_DEFINE_FADE] = &&op_OP_DEFINE_FADE;
        table_initialized = true;
    }
#endif

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define PUSH(val) (*(top++) = val)
#define POP() (*(--top))

#define CHECK_TIMERS() \
    if (!within_timers.empty()) { \
        if (++timer_counter >= 16) { \
            timer_counter = 0; \
            auto now = std::chrono::steady_clock::now(); \
            auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count(); \
            if (ms >= within_timers.back().end_time_ms) { \
                ip = within_timers.back().fallback_ip; \
                within_timers.pop_back(); \
            } \
        } \
    }

#ifdef _MSC_VER
    #define TARGET(op) case op:
    #define NEXT_CODE() do { CHECK_TIMERS(); goto loop_start; } while(0)
#else
    #define TARGET(op) op_##op:
    #define NEXT_CODE() do { CHECK_TIMERS(); goto *dispatch_table[READ_BYTE()]; } while(0)
#endif

#ifdef _MSC_VER
loop_start:
    switch (READ_BYTE()) {
#else
    NEXT_CODE();
#endif

#ifndef _MSC_VER
op_OP_UNKNOWN:
    return false;
#endif

TARGET(OP_CONSTANT)
    PUSH(frame->function->chunk.constants[READ_SHORT()]);
    NEXT_CODE();

TARGET(OP_NIL)
    PUSH(SapphireValue());
    NEXT_CODE();

TARGET(OP_TRUE)
    PUSH(SapphireValue(true));
    NEXT_CODE();

TARGET(OP_FALSE)
    PUSH(SapphireValue(false));
    NEXT_CODE();

TARGET(OP_POP)
    top--;
    NEXT_CODE();

TARGET(OP_DUP)
    PUSH(top[-1]);
    NEXT_CODE();

TARGET(OP_GET_LOCAL) {
    uint8_t slot = READ_BYTE();
    PUSH(evaluate_fade(slots[slot]));
    NEXT_CODE();
}

TARGET(OP_SET_LOCAL)
    slots[READ_BYTE()] = top[-1];
    NEXT_CODE();

TARGET(OP_GET_GLOBAL) {
    name_tmp = (ObjString*)frame->function->chunk.constants[READ_SHORT()].as.obj;
    auto it = globals.find(name_tmp->chars);
    if (it == globals.end()) {
        if (!this->soft_mode) {
            std::cerr << "Runtime Error: Undefined variable '" << name_tmp->chars << "'." << std::endl;
            return false;
        }
        PUSH(SapphireValue());
    } else PUSH(evaluate_fade(it->second));
    NEXT_CODE();
}

TARGET(OP_DEFINE_GLOBAL) {
    name_tmp = (ObjString*)frame->function->chunk.constants[READ_SHORT()].as.obj;
    globals[name_tmp->chars] = POP();
    NEXT_CODE();
}

TARGET(OP_SET_GLOBAL) {
    name_tmp = (ObjString*)frame->function->chunk.constants[READ_SHORT()].as.obj;
    if (globals.find(name_tmp->chars) == globals.end()) {
        if (!this->soft_mode) std::cerr << "Runtime Error: Undefined variable '" << name_tmp->chars << "'. Variables must be declared with 'var' or 'const'." << std::endl;
        return false;
    }
    globals[name_tmp->chars] = top[-1];
    NEXT_CODE();
}

TARGET(OP_GET_PROPERTY) {
    if (top[-1].type != ValType::VAL_OBJ) {
        if (!this->soft_mode) return false;
        top[-1] = SapphireValue();
    } else {
        Obj* obj = top[-1].as.obj;
        name_tmp = (ObjString*)frame->function->chunk.constants[READ_SHORT()].as.obj;
        if (obj->type == OBJ_MAP) {
            ObjMap* map = (ObjMap*)obj;
            auto it = map->items.find(name_tmp->chars);
            if (it != map->items.end()) top[-1] = it->second;
            else top[-1] = SapphireValue();
        } else if (obj->type == OBJ_CLASS) {
            ObjClass* klass = (ObjClass*)obj;
            auto it_m = klass->methods.find(name_tmp->chars);
            if (it_m != klass->methods.end()) {
                top[-1] = it_m->second;
            } else {
                top[-1] = SapphireValue();
            }
        } else {
            ObjInstance* instance = (ObjInstance*)obj;
            auto it_f = instance->fields.find(name_tmp->chars);
            if (it_f != instance->fields.end()) {
                top[-1] = it_f->second;
            } else {
                ObjClass* klass = instance->klass;
                bool found_method = false;
                while (klass != nullptr) {
                    auto it_m = klass->methods.find(name_tmp->chars);
                    if (it_m != klass->methods.end()) {
                        top[-1] = new_bound_method(this, top[-1], it_m->second, klass);
                        found_method = true;
                        break;
                    }
                    klass = klass->superclass;
                }
                if (!found_method) {
                    top[-1] = SapphireValue();
                }
            }
        }
    }
    NEXT_CODE();
}

TARGET(OP_SET_PROPERTY) {
    Obj* obj = top[-2].as.obj;
    name_tmp = (ObjString*)frame->function->chunk.constants[READ_SHORT()].as.obj;
    if (obj->type == OBJ_MAP) {
        ObjMap* map = (ObjMap*)obj;
        map->items[name_tmp->chars] = top[-1];
        write_barrier(obj, top[-1]);
    } else {
        ObjInstance* instance = (ObjInstance*)obj;
        instance->fields[name_tmp->chars] = top[-1];
        write_barrier(obj, top[-1]);
    }
    val_tmp = POP(); top--; PUSH(val_tmp);
    NEXT_CODE();
}

TARGET(OP_EQUAL) {
    {
        SapphireValue b = POP(); SapphireValue a = POP();
        bool isEqual = false;
        if (a.type == b.type) {
            if (a.type == ValType::VAL_OBJ && b.type == ValType::VAL_OBJ) {
                Obj* objA = a.as.obj;
                Obj* objB = b.as.obj;
                if (objA && objB && objA->type == OBJ_STRING && objB->type == OBJ_STRING) {
                    isEqual = (static_cast<ObjString*>(objA)->chars == static_cast<ObjString*>(objB)->chars);
                } else {
                    isEqual = (objA == objB);
                }
            } else {
                isEqual = values_equal(a, b);
            }
        }
        PUSH(SapphireValue(isEqual));
    }
    NEXT_CODE();
}

TARGET(OP_GREATER) {
    top[-2] = SapphireValue(top[-2].as.number > top[-1].as.number);
    top--;
    NEXT_CODE();
}

TARGET(OP_LESS) {
    top[-2] = SapphireValue(top[-2].as.number < top[-1].as.number);
    top--;
    NEXT_CODE();
}

TARGET(OP_ADD) {
    auto& v1 = top[-1]; auto& v2 = top[-2];
    if (v1.type == ValType::VAL_NUMBER && v2.type == ValType::VAL_NUMBER) {
        top[-2].as.number += top[-1].as.number;
        top--;
    } else if (v1.type == ValType::VAL_OBJ || v2.type == ValType::VAL_OBJ) {
        std::string s2 = valueToStringC(top[-2]);
        std::string s1 = valueToStringC(top[-1]);
        bool parsed = false;
        try {
            size_t p2 = 0, p1 = 0;
            double d2 = std::stod(s2, &p2);
            double d1 = std::stod(s1, &p1);
            if (p2 == s2.length() && p1 == s1.length()) {
                top -= 2; PUSH(SapphireValue(d2 + d1));
                parsed = true;
            }
        } catch (...) {}
        if (!parsed) {
            top -= 2; PUSH(new_string(this, s2 + s1));
        }
    } else { if (!this->soft_mode) return false; top -= 2; PUSH(SapphireValue()); }
    NEXT_CODE();
}

TARGET(OP_SUBTRACT) { double b = valueToDoubleC(POP()); double a = valueToDoubleC(POP()); PUSH(SapphireValue(a - b)); NEXT_CODE(); }
TARGET(OP_MULTIPLY) { double b = valueToDoubleC(POP()); double a = valueToDoubleC(POP()); PUSH(SapphireValue(a * b)); NEXT_CODE(); }
TARGET(OP_DIVIDE)   { double b = valueToDoubleC(POP()); double a = valueToDoubleC(POP()); PUSH(SapphireValue(a / b)); NEXT_CODE(); }
TARGET(OP_MODULO)   { double b = valueToDoubleC(POP()); double a = valueToDoubleC(POP()); PUSH(SapphireValue(std::fmod(a, b))); NEXT_CODE(); }

TARGET(OP_BITWISE_AND) { int64_t b = (int64_t)valueToDoubleC(POP()); int64_t a = (int64_t)valueToDoubleC(POP()); PUSH(SapphireValue((double)(a & b))); NEXT_CODE(); }
TARGET(OP_BITWISE_OR)  { int64_t b = (int64_t)valueToDoubleC(POP()); int64_t a = (int64_t)valueToDoubleC(POP()); PUSH(SapphireValue((double)(a | b))); NEXT_CODE(); }
TARGET(OP_BITWISE_XOR) { int64_t b = (int64_t)valueToDoubleC(POP()); int64_t a = (int64_t)valueToDoubleC(POP()); PUSH(SapphireValue((double)(a ^ b))); NEXT_CODE(); }
TARGET(OP_LEFT_SHIFT)  { int64_t b = (int64_t)valueToDoubleC(POP()); int64_t a = (int64_t)valueToDoubleC(POP()); PUSH(SapphireValue((double)(a << b))); NEXT_CODE(); }
TARGET(OP_RIGHT_SHIFT) { int64_t b = (int64_t)valueToDoubleC(POP()); int64_t a = (int64_t)valueToDoubleC(POP()); PUSH(SapphireValue((double)(a >> b))); NEXT_CODE(); }
TARGET(OP_BITWISE_NOT) { int64_t a = (int64_t)valueToDoubleC(top[-1]); top[-1] = SapphireValue((double)(~a)); NEXT_CODE(); }

TARGET(OP_NOT)
    top[-1] = SapphireValue(is_falsey(top[-1]));
    NEXT_CODE();

TARGET(OP_NEGATE)
    if (top[-1].type == ValType::VAL_NUMBER) top[-1].as.number *= -1;
    NEXT_CODE();

TARGET(OP_PRINT) {
    print_value(POP());
    std::cout << std::endl;
    NEXT_CODE();
}

TARGET(OP_JUMP)
    ip += READ_SHORT();
    NEXT_CODE();

TARGET(OP_JUMP_IF_FALSE) {
    uint16_t offset = READ_SHORT();
    if (is_falsey(top[-1])) ip += offset;
    NEXT_CODE();
}

TARGET(OP_JUMP_IF_NIL) {
    uint16_t offset = READ_SHORT();
    if (top[-1].type == ValType::VAL_NIL) ip += offset;
    NEXT_CODE();
}

TARGET(OP_JUMP_IF_NOT_NIL) {
    uint16_t offset = READ_SHORT();
    if (top[-1].type != ValType::VAL_NIL) ip += offset;
    NEXT_CODE();
}

TARGET(OP_LOOP)
    stack_top = top;
    step_gc();
    ip -= READ_SHORT();
    NEXT_CODE();

TARGET(OP_CALL) {
    int arg_count = READ_BYTE();
    SapphireValue callee = top[-arg_count - 1];
    if (callee.type == ValType::VAL_OBJ && callee.as.obj->type == OBJ_CLOSURE) {
        ObjClosure* closure = static_cast<ObjClosure*>(callee.as.obj);
        if (arg_count == closure->function->arity && frame_count < FRAMES_MAX && !closure->function->is_async) {
            frame->ip = ip;
            CallFrame* next_frame = &frames[frame_count++];
            next_frame->function = closure->function;
            next_frame->ip = closure->function->chunk.code.data();
            next_frame->slots = top - arg_count - 1;
            frame = next_frame;
            ip = frame->ip; 
            slots = frame->slots;
            NEXT_CODE();
        }
    }
    stack_top = top;
    step_gc();
    frame->ip = ip;
    if (!call_value(callee, arg_count)) return false;
    frame = &frames[frame_count - 1];
    ip = frame->ip; slots = frame->slots; top = stack_top;
    NEXT_CODE();
}

TARGET(OP_CLOSURE) {
    func_tmp = (ObjFunction*)frame->function->chunk.constants[READ_SHORT()].as.obj;
    PUSH(new_closure(this, func_tmp));
    NEXT_CODE();
}

TARGET(OP_RETURN) {
    val_tmp = POP();
    frame_count--;
    if (frame_count == target_frame_count) { 
        stack_top = top; 
        if (this->current_promise != nullptr) {
            this->current_promise->state = PromiseState::FULFILLED;
            this->current_promise->value = val_tmp;
            for (auto* awaiter : this->current_promise->awaiters) {
                this->event_loop_queue.push_back(awaiter);
            }
            this->current_promise->awaiters.clear();
        }
        return true; 
    }
    top = frame->slots;
    PUSH(val_tmp);
    frame = &frames[frame_count - 1];
    ip = frame->ip; slots = frame->slots;
    NEXT_CODE();
}

TARGET(OP_BUILD_ARRAY) {
    {
        uint8_t count = READ_BYTE();
        auto arr = new_array(g_current_vm);
        for (int i = 0; i < count; i++) arr->elements.push_back(top[-count + i]);
        top -= count; PUSH(arr);
    }
    NEXT_CODE();
}

TARGET(OP_SPREAD_ARRAY) {
    {
        SapphireValue src = POP();
        SapphireValue target = top[-1];
        if (is_obj_type(target, OBJ_ARRAY) && 
            is_obj_type(src, OBJ_ARRAY)) {
            auto arr_target = static_cast<ObjArray*>(target.as.obj);
            auto arr_src = static_cast<ObjArray*>(src.as.obj);
            arr_target->elements.insert(arr_target->elements.end(), arr_src->elements.begin(), arr_src->elements.end());
        } else {
            if (!this->soft_mode) std::cerr << "Runtime Error: Cannot spread non-array." << std::endl;
        }
    }
    NEXT_CODE();
}

TARGET(OP_BUILD_MAP) {
    {
        uint8_t count = READ_BYTE();
        ObjMap* map_obj = new_map(this);
        for (int i = 0; i < count; i++) {
            SapphireValue val = POP();
            SapphireValue key = POP();
            std::string key_str = static_cast<ObjString*>(key.as.obj)->chars;
            map_obj->items[key_str] = val;
        }
        PUSH(map_obj);
    }
    NEXT_CODE();
}

TARGET(OP_GET_SUBSCRIPT) {
    {
        SapphireValue index = POP();
        SapphireValue collection = POP();
        
        if (is_obj_type(collection, OBJ_ARRAY)) {
            auto arr = static_cast<ObjArray*>(collection.as.obj);
            if (index.type == ValType::VAL_NUMBER) { 
                int idx = (int)index.as.number;
                if (idx >= 0 && idx < arr->elements.size()) {
                    PUSH(arr->elements[idx]);
                } else {
                    if (!this->soft_mode) { std::cerr << "Runtime Error: Array index out of bounds." << std::endl; return false; }
                    PUSH(SapphireValue());
                }
            } else {
                if (!this->soft_mode) { std::cerr << "Runtime Error: Array index must be a number." << std::endl; return false; }
                PUSH(SapphireValue());
            }
        } else if (collection.type == ValType::VAL_OBJ && collection.as.obj->type == OBJ_MAP) {
            ObjMap* map_obj = static_cast<ObjMap*>(collection.as.obj);
            if (index.type == ValType::VAL_OBJ && index.as.obj->type == OBJ_STRING) {
                std::string key_str = static_cast<ObjString*>(index.as.obj)->chars;
                auto it = map_obj->items.find(key_str);
                if (it != map_obj->items.end()) {
                    PUSH(it->second);
                } else {
                    PUSH(SapphireValue());
                }
            } else {
                if (!this->soft_mode) { std::cerr << "Runtime Error: Map key must be a string." << std::endl; return false; }
                PUSH(SapphireValue());
            }
        } else {
            if (!this->soft_mode) { std::cerr << "Runtime Error: Cannot subscript this type." << std::endl; return false; }
            PUSH(SapphireValue());
        }
    }
    NEXT_CODE();
}

TARGET(OP_SET_SUBSCRIPT) {
    {
        SapphireValue value = POP();
        SapphireValue index = POP();
        SapphireValue collection = POP();
        
        if (is_obj_type(collection, OBJ_ARRAY)) {
            auto arr = static_cast<ObjArray*>(collection.as.obj);
            if (index.type == ValType::VAL_NUMBER) {
                int idx = (int)index.as.number;
                if (idx >= 0 && idx <= arr->elements.size()) {
                    if (idx == arr->elements.size()) {
                        arr->elements.push_back(value);
                    } else {
                        arr->elements[idx] = value;
                    }
                } else {
                    if (!this->soft_mode) { std::cerr << "Runtime Error: Array index out of bounds." << std::endl; return false; }
                }
            } else {
                if (!this->soft_mode) { std::cerr << "Runtime Error: Array index must be a number." << std::endl; return false; }
            }
        } else if (collection.type == ValType::VAL_OBJ && collection.as.obj->type == OBJ_MAP) {
            ObjMap* map_obj = static_cast<ObjMap*>(collection.as.obj);
            if (index.type == ValType::VAL_OBJ && index.as.obj->type == OBJ_STRING) {
                std::string key_str = static_cast<ObjString*>(index.as.obj)->chars;
                map_obj->items[key_str] = value;
                write_barrier((Obj*)map_obj, value);
            } else {
                if (!this->soft_mode) { std::cerr << "Runtime Error: Map key must be a string." << std::endl; return false; }
            }
        } else {
            if (!this->soft_mode) { std::cerr << "Runtime Error: Cannot subscript this type." << std::endl; return false; }
        }
        PUSH(value);
    }
    NEXT_CODE();
}

TARGET(OP_IMPORT) {
    name_tmp = (ObjString*)frame->function->chunk.constants[READ_SHORT()].as.obj;
    frame->ip = ip; stack_top = top;
    
    src_tmp = find_and_load_module(name_tmp->chars, resolved_path_tmp);
    if (src_tmp.empty()) {
        std::cerr << "[SAPPHIRE ERROR] Module '" << name_tmp->chars << "' not found." << std::endl;
        return false;
    }
    
    try {
        std::string dir = std::filesystem::path(resolved_path_tmp).parent_path().string();
        if (std::find(module_search_paths.begin(), module_search_paths.end(), dir) == module_search_paths.end()) {
            module_search_paths.push_back(dir);
        }
    } catch (...) {}
    
    if (loaded_modules.find(resolved_path_tmp) != loaded_modules.end()) {
        PUSH(SapphireValue());
        NEXT_CODE();
    }
    
    loaded_modules.insert(resolved_path_tmp);
    
    func_tmp = compile(this, src_tmp);
    if (!func_tmp) return false;
    PUSH(new_closure(this, func_tmp));
    call_value(top[-1], 0);
    frame = &frames[frame_count - 1];
    ip = frame->ip; slots = frame->slots; top = stack_top;
    NEXT_CODE();
}

TARGET(OP_MAKE_NAMED_ARG) {
    {
        SapphireValue value = POP();
        SapphireValue name = POP();
        ObjNamedArg* arg = new_named_arg(this, static_cast<ObjString*>(name.as.obj), value);
        PUSH(arg);
    }
    NEXT_CODE();
}

TARGET(OP_AWAIT) {
    {
        SapphireValue promise_val = top[-1]; // Peek at it
        if (promise_val.type != ValType::VAL_OBJ || promise_val.as.obj->type != OBJ_PROMISE) {
            if (!this->soft_mode) std::cerr << "Runtime Error: Can only await promises." << std::endl;
            return false;
        }
        
        ObjPromise* promise = (ObjPromise*)promise_val.as.obj;
        
        if (promise->state == PromiseState::FULFILLED) {
            POP(); // Remove promise
            PUSH(promise->value); // Push result
            // proceed to NEXT_CODE below
        } else if (promise->state == PromiseState::REJECTED) {
            if (!this->soft_mode) std::cerr << "Runtime Error: Unhandled promise rejection." << std::endl;
            return false;
        } else {
            // Promise is PENDING. We must suspend the current coroutine.
            if (this->current_promise == nullptr) {
                this->current_promise = new_promise(this);
                this->current_promise->function = nullptr; // Main script
            }
            
            this->current_promise->saved_stack.clear();
            for (SapphireValue* s = stack; s < top; s++) this->current_promise->saved_stack.push_back(*s);
            
            this->current_promise->saved_frames.clear();
            for (int i = 0; i < frame_count; i++) this->current_promise->saved_frames.push_back(frames[i]);
            
            promise->awaiters.push_back(this->current_promise);
            
            frame->ip = ip - 1; // Point back to OP_AWAIT
            event_loop_queue.push_back(this->current_promise);
            return true; // Exits run() cleanly.
        }
    }
    NEXT_CODE();
}TARGET(OP_ASYNC_CALL) {
    // TODO: implement OP_ASYNC_CALL
    NEXT_CODE();
}

TARGET(OP_INHERIT) {
    {
        // Stack has: [superclass, subclass]
        SapphireValue subclass_val = POP();
        SapphireValue superclass_val = top[-1];
        
        if (superclass_val.type != ValType::VAL_OBJ || superclass_val.as.obj->type != OBJ_CLASS) {
            std::cerr << "Runtime Error: Superclass must be a class." << std::endl;
            return false;
        }
        ObjClass* superclass = static_cast<ObjClass*>(superclass_val.as.obj);
        ObjClass* subclass = static_cast<ObjClass*>(subclass_val.as.obj);
        
        subclass->superclass = superclass;
        
        // Replace the superclass on the stack with the subclass so define_variable binds the subclass
        top[-1] = subclass_val;
    }
    NEXT_CODE();
}

TARGET(OP_SPAWN) {
    {
        SapphireValue func_val = POP();
        
        if (func_val.type != ValType::VAL_OBJ || (func_val.as.obj->type != OBJ_CLOSURE && func_val.as.obj->type != OBJ_FUNCTION)) {
            std::cerr << "Runtime Error: Can only spawn functions or closures." << std::endl;
            return false;
        }
        
        ObjFunction* function = nullptr;
        Obj* obj = func_val.as.obj;
        if (obj->type == OBJ_CLOSURE) {
            function = static_cast<ObjClosure*>(obj)->function;
        } else {
            function = static_cast<ObjFunction*>(obj);
        }
        
        // Serialize function
        std::stringstream ss;
        serialize_function_to_stream(function, this, ss);
        std::string bytecode = ss.str();
        
        // Spawn a C++ thread
        std::thread worker([bytecode, config = this->config]() {
            VM* thread_vm = new VM(config);
            std::stringstream in_ss(bytecode);
            
            ObjFunction* thread_function = deserialize_function_from_stream(thread_vm, in_ss);
            if (thread_function) {
                thread_vm->push(SapphireValue(thread_function));
                thread_vm->call_and_run(thread_function);
            }
            
            delete thread_vm;
        });
        
        worker.detach();
    }
    // Do NOT push since spawn is a statement!
    NEXT_CODE();
}

TARGET(OP_GET_SUPER) {
    {
        name_tmp = (ObjString*)frame->function->chunk.constants[READ_SHORT()].as.obj;
        ObjClass* superclass = frame->function->owner_class->superclass;
        if (superclass == nullptr) {
            std::cerr << "Runtime Error: Cannot use 'super' in a class with no superclass." << std::endl;
            return false;
        }
        
        SapphireValue instance_val = top[-1];
        ObjClass* current_class = superclass;
        bool found_method = false;
        
        while (current_class != nullptr) {
            auto it_m = current_class->methods.find(name_tmp->chars);
            if (it_m != current_class->methods.end()) {
                top[-1] = new_bound_method(this, instance_val, it_m->second, current_class);
                found_method = true;
                break;
            }
            current_class = current_class->superclass;
        }
        
        if (!found_method) {
            std::cerr << "Runtime Error: Undefined property '" << name_tmp->chars << "'." << std::endl;
            return false;
        }
    }
    NEXT_CODE();
}

TARGET(OP_TRY_START) {
    uint16_t offset = READ_SHORT();
    if (catch_count < 64) {
        CatchBlock& block = catch_blocks[catch_count++];
        block.frame_count = frame_count;
        block.stack_top = top;
        block.catch_ip = ip + offset;
    } else {
        std::cerr << "[SAPPHIRE ERROR] Too many nested try blocks." << std::endl;
        return false;
    }
    
    {
        UndoBackup backup;
        backup.globals = globals;
        backup.locals.assign(stack, top);
        backup.stack_top = top;
        backup.frame_count = frame_count;
        undo_stack.push_back(std::move(backup));
    }
    
    NEXT_CODE();
}

TARGET(OP_TRY_END) {
    if (catch_count > 0) catch_count--;
    if (!undo_stack.empty()) undo_stack.pop_back();
    NEXT_CODE();
}

TARGET(OP_UNDO) {
    if (undo_stack.empty()) {
        std::cerr << "Runtime Error: 'undo' called outside of a try block.\n";
        return false;
    }
    
    UndoBackup& backup = undo_stack.back();
    globals = backup.globals;
    for (size_t i = 0; i < backup.locals.size(); ++i) {
        stack[i] = backup.locals[i];
    }
    top = stack + backup.locals.size();
    frame_count = backup.frame_count;
    frame = &frames[frame_count - 1];
    slots = frame->slots;
    
    if (catch_count > 0) {
        CatchBlock block = catch_blocks[--catch_count];
        uint8_t* jump_op = block.catch_ip - 3;
        if (*jump_op == OP_JUMP) {
            uint16_t jump_offset = (jump_op[1] << 8) | jump_op[2];
            ip = jump_op + 3 + jump_offset;
        } else {
            ip = block.catch_ip;
        }
    }
    
    undo_stack.pop_back();
    NEXT_CODE();
}

TARGET(OP_THROW) {
    {
        SapphireValue err = POP();
        if (catch_count == 0) {
            std::cerr << "Unhandled Exception: ";
            print_value(err);
            std::cerr << "\nStack trace:\n";
            for (int i = frame_count - 1; i >= 0; i--) {
                CallFrame* f = &frames[i];
                std::cerr << "  in " << (f->function->name != nullptr ? f->function->name->chars : "<script>") << "\n";
            }
            return false;
        }
        
        CatchBlock block = catch_blocks[--catch_count];
        frame_count = block.frame_count;
        frame = &frames[frame_count - 1];
        top = block.stack_top;
        ip = block.catch_ip;
        slots = frame->slots;
        
        PUSH(err); // Push the exception for the catch block to use
    }
    NEXT_CODE();
}

TARGET(OP_GET_ITERATOR) {
    PUSH(SapphireValue((double)0));
    NEXT_CODE();
}

TARGET(OP_WITHIN_START) {
    uint16_t fallback_offset = READ_SHORT();
    SapphireValue limit_val = POP();
    if (limit_val.type != ValType::VAL_NUMBER) {
        std::cerr << "Runtime Error: within time limit must be a number." << std::endl;
        return false;
    }
    
    WithinTimer timer;
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
    timer.end_time_ms = ms + (uint64_t)limit_val.as.number;
    timer.fallback_ip = ip + fallback_offset;
    
    within_timers.push_back(timer);
    NEXT_CODE();
}

TARGET(OP_WITHIN_END) {
    if (!within_timers.empty()) {
        within_timers.pop_back();
    }
    NEXT_CODE();
}

TARGET(OP_EVERY_TICK) {
    SapphireValue time_val = POP();
    if (time_val.type == ValType::VAL_NUMBER) {
        std::this_thread::sleep_for(std::chrono::milliseconds((int)time_val.as.number));
        timer_counter = 16; // Force timer check after sleep
    }
    NEXT_CODE();
}

TARGET(OP_DEFINE_FADE) {
    SapphireValue initial_val = POP();
    SapphireValue curve_val = POP();
    SapphireValue time_val = POP();
    
    if (time_val.type != ValType::VAL_NUMBER || curve_val.type != ValType::VAL_OBJ || curve_val.as.obj->type != OBJ_STRING) {
        std::cerr << "Runtime Error: Invalid fade arguments." << std::endl;
        return false;
    }
    
    ObjString* curve_str = (ObjString*)curve_val.as.obj;
    ObjFade* fade = new_fade(this, initial_val, time_val.as.number, curve_str->chars);
    
    auto now = std::chrono::steady_clock::now();
    fade->created_at_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
    
    PUSH(SapphireValue((Obj*)fade));
    NEXT_CODE();
}

TARGET(OP_ITER_NEXT_IN) {
    {
        uint16_t offset = READ_SHORT();
        SapphireValue state_val = top[-1];
        SapphireValue iterable_val = top[-2];
        
        int index = (int)state_val.as.number;
        
        if (is_obj_type(iterable_val, OBJ_ARRAY)) {
            auto arr = static_cast<ObjArray*>(iterable_val.as.obj);
            if (index < arr->elements.size()) {
                PUSH(SapphireValue((double)index));
                    top[-2].as.number += 1.0;
            } else {
                ip += offset;
            }
        } else if (iterable_val.type == ValType::VAL_OBJ) {
            Obj* obj = iterable_val.as.obj;
            if (obj->type == OBJ_MAP) {
                ObjMap* map = static_cast<ObjMap*>(obj);
                if (index < map->items.size()) {
                    auto it = map->items.begin();
                    std::advance(it, index);
                    PUSH(new_string(this, it->first));
                    top[-2].as.number += 1.0;
                } else {
                    ip += offset;
                }
            } else if (obj->type == OBJ_STRING) {
                ObjString* str = static_cast<ObjString*>(obj);
                if (index < str->chars.length()) {
                    PUSH(SapphireValue((double)index));
                    top[-2].as.number += 1.0;
                } else {
                    ip += offset;
                }
            } else {
                ip += offset;
            }
        } else {
            ip += offset;
        }
    }
    NEXT_CODE();
}

TARGET(OP_ITER_NEXT_OF) {
    {
        uint16_t offset = READ_SHORT();
        SapphireValue state_val = top[-1];
        SapphireValue iterable_val = top[-2];
        
        int index = (int)state_val.as.number;
        
        if (is_obj_type(iterable_val, OBJ_ARRAY)) {
            auto arr = static_cast<ObjArray*>(iterable_val.as.obj);
            if (index < arr->elements.size()) {
                PUSH(arr->elements[index]);
                    top[-2].as.number += 1.0;
            } else {
                ip += offset;
            }
        } else if (iterable_val.type == ValType::VAL_OBJ) {
            Obj* obj = iterable_val.as.obj;
            if (obj->type == OBJ_MAP) {
                ObjMap* map = static_cast<ObjMap*>(obj);
                if (index < map->items.size()) {
                    auto it = map->items.begin();
                    std::advance(it, index);
                    PUSH(it->second);
                    top[-2].as.number += 1.0;
                } else {
                    ip += offset;
                }
            } else if (obj->type == OBJ_STRING) {
                ObjString* str = static_cast<ObjString*>(obj);
                if (index < str->chars.length()) {
                    PUSH(new_string(this, std::string(1, str->chars[index])));
                    top[-2].as.number += 1.0;
                } else {
                    ip += offset;
                }
            } else {
                ip += offset;
            }
        } else {
            ip += offset;
        }
    }
    NEXT_CODE();
}

#ifdef _MSC_VER
    default: return false;
    }
#endif

#undef READ_BYTE
#undef READ_SHORT
#undef PUSH
#undef POP
#undef NEXT_CODE
#undef top
}

bool VM::run_function(ObjFunction* function) {
    // std::cout << "  [VM DEBUG] Entrando em run_function..." << std::endl;
    if (function == nullptr) return false;
    resetStack();
    push(function);
    if (!call(function, 0)) {
        return false;
    }
    bool result = run();
    // std::cout << "  [VM DEBUG] Saindo de run_function." << std::endl;
    return result;
}

static bool parse_top_memory_limit_mb(const std::string& source, size_t& out_limit_mb) {
    size_t pos = 0;
    const size_t len = source.size();

    auto trim_left = [&](size_t& start) {
        while (start < len && (source[start] == ' ' || source[start] == '\t' || source[start] == '\r')) {
            start++;
        }
    };

    const std::string keyword = "var";
    const std::string name = "MEMORY_LIMIT";

    while (pos < len) {
        size_t line_start = pos;
        size_t line_end = source.find('\n', pos);
        if (line_end == std::string::npos) {
            line_end = len;
        }

        size_t token_start = line_start;
        trim_left(token_start);
        if (token_start >= line_end) {
            pos = line_end == len ? len : line_end + 1;
            continue;
        }

        if (source.compare(token_start, 2, "//") == 0) {
            pos = line_end == len ? len : line_end + 1;
            continue;
        }

        if (source.compare(token_start, 2, "/*") == 0) {
            size_t comment_end = source.find("*/", token_start + 2);
            if (comment_end == std::string::npos) return false;
            pos = comment_end + 2;
            continue;
        }

        if (token_start + keyword.size() <= line_end && source.compare(token_start, keyword.size(), keyword) == 0) {
            size_t after_keyword = token_start + keyword.size();
            if (after_keyword < line_end && isspace(static_cast<unsigned char>(source[after_keyword]))) {
                size_t var_name_start = after_keyword;
                trim_left(var_name_start);
                if (var_name_start + name.size() <= line_end && source.compare(var_name_start, name.size(), name) == 0) {
                    size_t value_pos = var_name_start + name.size();
                    trim_left(value_pos);
                    if (value_pos < line_end && source[value_pos] == '=') {
                        value_pos++;
                        trim_left(value_pos);
                        size_t value_start = value_pos;
                        while (value_pos < line_end && isdigit(static_cast<unsigned char>(source[value_pos]))) {
                            value_pos++;
                        }
                        if (value_start == value_pos) {
                            return false;
                        }

                        size_t limit_mb = 0;
                        try {
                            limit_mb = std::stoull(source.substr(value_start, value_pos - value_start));
                        } catch (...) {
                            return false;
                        }

                        trim_left(value_pos);
                        if (value_pos < line_end) {
                            if (source[value_pos] == ';') {
                                value_pos++;
                                trim_left(value_pos);
                            }
                            // Permitir comentÃ¡rios apÃ³s o ponto e vÃ­rgula
                            if (value_pos < line_end && source.compare(value_pos, 2, "//") != 0) {
                                return false;
                            }
                        }

                        if (limit_mb == 0) {
                            return false;
                        }

                        out_limit_mb = limit_mb;
                        std::cout << "[VM] Memory limit set to " << limit_mb << " MB from script" << std::endl;
                        return true;
                    }
                }
            }
        }

        pos = line_end == len ? len : line_end + 1;
    }

    return false;
}

SapphireValue VM::interpret(const std::string& source) {
    size_t memory_limit_mb;
    if (parse_top_memory_limit_mb(source, memory_limit_mb)) {
        max_memory_limit = memory_limit_mb * 1024ull * 1024ull;
    }

    Preprocessor prep;
    std::string processed_source = prep.process(source);

    ObjFunction* function = compile(this, processed_source);
    if (function == nullptr) return {};

    resetStack();
    push(function);

    if (!call(function, 0)) return {};

    bool result = run();

    // Event Loop
    while (!event_loop_queue.empty()) {
        ObjPromise* next_promise = event_loop_queue.front();
        event_loop_queue.erase(event_loop_queue.begin());
        
        if (next_promise->state != PromiseState::PENDING) continue;
        
        this->current_promise = next_promise;
        
        if (next_promise->saved_frames.empty() && next_promise->function != nullptr) {
            // First time running this coroutine!
            resetStack();
            push(SapphireValue(next_promise->function));
            for (const auto& arg : next_promise->args) push(arg);
            
            CallFrame* frame = &frames[frame_count++];
            frame->function = next_promise->function;
            frame->ip = &next_promise->function->chunk.code[0];
            frame->slots = stack;
        } else if (!next_promise->saved_frames.empty()) {
            // Resuming!
            stack_top = stack;
            for (auto v : next_promise->saved_stack) *stack_top++ = v;
            frame_count = 0;
            for (auto f : next_promise->saved_frames) frames[frame_count++] = f;
        } else {
            // Dummy main promise
            continue;
        }
        
        run();
    }
    
    this->current_promise = nullptr;

    if (result && stack_top > stack) {
        return pop();
    }
    return {};
}

void VM::resetStack() {
    stack_top = stack;
    frame_count = 0;
}

SapphireValue VM::getGlobal(const std::string& name) {
    auto it = globals.find(name);
    if (it != globals.end()) {
        return it->second;
    }
    return {};
}

// --- FunÃ§Ãµes do Coletor de Lixo ---
void VM::mark_object(Obj* object) {
    if (object == nullptr || object->is_marked) return;

    object->is_marked = true;
    gray_stack.push_back(object);
}

void VM::mark_value(SapphireValue value) {
    if (value.type == ValType::VAL_OBJ) {
        mark_object(value.as.obj);
    } else if (is_obj_type(value, OBJ_ARRAY)) {
        auto array = static_cast<ObjArray*>(value.as.obj);
        for (SapphireValue& val : array->elements) {
            mark_value(val);
        }
    }
}

void VM::blacken_object(Obj* object) {
    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            mark_object((Obj*)closure->function);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            mark_object((Obj*)function->name);
            for (SapphireValue& constant : function->chunk.constants) {
                mark_value(constant);
            }
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            mark_object((Obj*)instance->klass);
            for (auto const& [key, val] : instance->fields) {
                mark_value(val);
            }
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            mark_object((Obj*)klass->name);
            for (auto const& [key, val] : klass->methods) {
                mark_value(val);
            }
            break;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            mark_value(bound->receiver);
            mark_value(bound->method);
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

void VM::mark_roots() {
    for (SapphireValue* slot = stack; slot < stack_top; slot++) {
        mark_value(*slot);
    }
    for (int i = 0; i < frame_count; i++) {
        mark_object((Obj*)frames[i].function);
    }
    for (auto const& [key, val] : globals) {
        mark_value(val);
    }
}

void VM::write_barrier(Obj* object, SapphireValue value) {
    if (gc_state == GCState::GC_TRACE) {
        if (object->is_marked) {
            mark_value(value);
        }
    }
}

void VM::step_gc() {
    if (gc_state == GCState::GC_IDLE) {
        if (bytes_allocated > next_gc_threshold) {
            gc_state = GCState::GC_MARK_ROOTS;
        } else {
            return;
        }
    }

    if (gc_state == GCState::GC_MARK_ROOTS) {
        mark_roots();
        gc_state = GCState::GC_TRACE;
        return;
    }

    if (gc_state == GCState::GC_TRACE) {
        int trace_limit = 500;
        while (!gray_stack.empty() && trace_limit > 0) {
            Obj* object = gray_stack.back();
            gray_stack.pop_back();
            blacken_object(object);
            trace_limit--;
        }
        if (gray_stack.empty()) {
            // Remark phase: rescan roots to catch anything mutated during tracing
            mark_roots();
            if (gray_stack.empty()) {
                gc_state = GCState::GC_SWEEP;
                sweep_previous = nullptr;
                sweep_current = objects;
            }
        }
        return;
    }

    if (gc_state == GCState::GC_SWEEP) {
        int sweep_limit = 500;
        while (sweep_current != nullptr && sweep_limit > 0) {
            Obj* object = sweep_current;
            if (object->is_marked) {
                object->is_marked = false;
                sweep_previous = object;
                sweep_current = object->next;
            } else {
                Obj* unreached = object;
                sweep_current = object->next;
                if (sweep_previous != nullptr) {
                    sweep_previous->next = sweep_current;
                } else {
                    objects = sweep_current;
                }
                
                size_t size = 0;
                switch (unreached->type) {
                    case OBJ_STRING: size = sizeof(ObjString); break;
                    case OBJ_FUNCTION: size = sizeof(ObjFunction); break;
                    case OBJ_NATIVE: size = sizeof(ObjNative); break;
                    case OBJ_CLOSURE: size = sizeof(ObjClosure); break;
                    case OBJ_CLASS: size = sizeof(ObjClass); break;
                    case OBJ_INSTANCE: size = sizeof(ObjInstance); break;
                    case OBJ_BOUND_METHOD: size = sizeof(ObjBoundMethod); break;
                    case OBJ_NAMED_ARG: size = sizeof(ObjNamedArg); break;
                }
                if (bytes_allocated >= size) bytes_allocated -= size;
                else bytes_allocated = 0;

                free_object(unreached);
            }
            sweep_limit--;
        }

        if (sweep_current == nullptr) {
            gc_state = GCState::GC_IDLE;
            next_gc_threshold = bytes_allocated * 2;
            if (next_gc_threshold < 1024 * 1024) next_gc_threshold = 1024 * 1024;
        }
        return;
    }
}

bool VM::call_and_run(ObjFunction* function) {
    if (function == nullptr) return false;

    SapphireValue* starting_stack = stack_top;

    push(function);
    if (!call(function, 0)) {
        stack_top = starting_stack;
        return false;
    }

    bool result = run();

    stack_top = starting_stack;
    frame_count = 0;

    return result;
}





