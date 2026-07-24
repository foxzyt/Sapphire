#include "builtins.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <iomanip>
#include "../../utils/utils.h"
#include "../object.h"
#include "../value.h"

static int next_mutex_id = 1;
static std::map<int, std::shared_ptr<std::mutex>> global_mutexes;
static std::mutex global_mutexes_lock;
static int next_thread_id = 1;
static std::map<int, std::thread> active_threads;
std::mutex thread_mutex;
static std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_c);
    std::stringstream ss;
    ss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

SapphireValue native_logger_info(int arg_count, SapphireValue* args) {
    std::cout << "\033[94m[" << get_current_timestamp() << "] [INFO]\033[0m ";
    for (int i = 0; i < arg_count; i++) {
        print_value(args[i]);
        if (i < arg_count - 1) std::cout << " ";
    }
    std::cout << "\n";
    return SapphireValue();
}

SapphireValue native_logger_warn(int arg_count, SapphireValue* args) {
    std::cout << "\033[93m[" << get_current_timestamp() << "] [WARN]\033[0m ";
    for (int i = 0; i < arg_count; i++) {
        print_value(args[i]);
        if (i < arg_count - 1) std::cout << " ";
    }
    std::cout << "\n";
    return SapphireValue();
}

SapphireValue native_logger_error(int arg_count, SapphireValue* args) {
    std::cout << "\033[91m[" << get_current_timestamp() << "] [ERROR]\033[0m ";
    for (int i = 0; i < arg_count; i++) {
        print_value(args[i]);
        if (i < arg_count - 1) std::cout << " ";
    }
    std::cout << "\n";
    return SapphireValue();
}

SapphireValue native_logger_debug(int arg_count, SapphireValue* args) {
    std::cout << "\033[90m[" << get_current_timestamp() << "] [DEBUG]\033[0m ";
    for (int i = 0; i < arg_count; i++) {
        print_value(args[i]);
        if (i < arg_count - 1) std::cout << " ";
    }
    std::cout << "\n";
    return SapphireValue();
}

SapphireValue native_spawn(int arg_count, SapphireValue* args) {
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

SapphireValue native_mutex_new(int arg_count, SapphireValue* args) {
    std::lock_guard<std::mutex> lock(global_mutexes_lock);
    int id = next_mutex_id++;
    global_mutexes[id] = std::make_shared<std::mutex>();
    return SapphireValue((double)id);
}

SapphireValue native_mutex_lock(int arg_count, SapphireValue* args) {
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

SapphireValue native_mutex_unlock(int arg_count, SapphireValue* args) {
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

SapphireValue native_join(int arg_count, SapphireValue* args) {
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

SapphireValue native_system_core_count(int arg_count, SapphireValue* args) {
    unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0) cores = 4;
    return SapphireValue((double)cores);
}

SapphireValue native_system_sleep(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) {
        return {};
    }
    int ms = static_cast<int>(args[0].as.number);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return {};
}

SapphireValue native_system_exec(int arg_count, SapphireValue* args) {
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

SapphireValue native_system_get_os(int arg_count, SapphireValue* args) {
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

SapphireValue native_system_get_env(int arg_count, SapphireValue* args) {
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

SapphireValue native_system_get_clipboard(int arg_count, SapphireValue* args) {
    std::string text = sf::Clipboard::getString().toAnsiString();
    return new_string(g_current_vm, text);
}

