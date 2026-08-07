#include "builtins.h"
#include "../object.h"
#include "../value.h"

SapphireValue native_lru_create(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) {
        return SapphireValue(new_lru(g_current_vm, 128));
    }
    return SapphireValue(new_lru(g_current_vm, (int)args[0].as.number));
}

SapphireValue native_lru_has(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_LRU || args[1].type != ValType::VAL_OBJ || args[1].as.obj->type != OBJ_STRING) return SapphireValue(false);
    ObjLRU* lru = (ObjLRU*)args[0].as.obj;
    ObjString* key = (ObjString*)args[1].as.obj;
    return SapphireValue(lru->items.find(key->chars) != lru->items.end());
}

SapphireValue native_lru_get(int arg_count, SapphireValue* args) {
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

SapphireValue native_lru_put(int arg_count, SapphireValue* args) {
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

SapphireValue native_value_to_string(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: valueToString() expects 1 argument." << std::endl;
        }
        return new_string(g_current_vm, "");
    }
    return new_string(g_current_vm, valueToStringC(args[0]));
}

SapphireValue native_array_push(int arg_count, SapphireValue* args) {
    if (arg_count != 2) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: push() expects 2 arguments." << std::endl;
        }
        return {};
    }
    if (is_obj_type(args[0], OBJ_ARRAY)) {
        auto array_obj = static_cast<ObjArray*>(args[0].as.obj);
        array_obj->elements.push_back(args[1]);
        return args[0];
    }
    if (!g_current_vm->soft_mode) {
        std::cerr << "Runtime Error: First argument to push() must be an array." << std::endl;
    }
    return {};
}

SapphireValue native_len(int arg_count, SapphireValue* args) {
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

SapphireValue native_evaluate(int arg_count, SapphireValue* args) {
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

SapphireValue native_debug_print_stack(int arg_count, SapphireValue* args) {
    std::cout << "--- SAPPHIRE STACK DUMP ---" << std::endl;
    for (SapphireValue* slot = g_current_vm->stack; slot < g_current_vm->stack_top; slot++) {
        std::cout << "[ ";
        print_value(*slot);
        std::cout << " ]" << std::endl;
    }
    std::cout << "--- END OF STACK ---" << std::endl;
    return {};
}

SapphireValue native_debug_dump_globals(int arg_count, SapphireValue* args) {
    std::cout << "--- SAPPHIRE GLOBALS DUMP ---" << std::endl;
    for (auto const& [name, value] : g_current_vm->globals) {
        std::cout << name << " => ";
        print_value(value);
        std::cout << std::endl;
    }
    return {};
}

SapphireValue native_get_quote(int arg_count, SapphireValue* args) {
    return new_string(g_current_vm, "\"");
}

SapphireValue native_color_hex_to_rgb(int arg_count, SapphireValue* args) {
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

SapphireValue native_check_collision(int arg_count, SapphireValue* args) {
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



static const auto clock_start_time = std::chrono::high_resolution_clock::now();

SapphireValue clock_native(int arg_count, SapphireValue* args) {
    if (arg_count != 0) return {};
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = now - clock_start_time;
    return diff.count();
}


SapphireValue assert_native(int arg_count, SapphireValue* args) {
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


SapphireValue io_readline_native(int arg_count, SapphireValue* args) {
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
