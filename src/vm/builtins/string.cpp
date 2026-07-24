#include "builtins.h"
#include "../object.h"
#include "../value.h"

SapphireValue native_string_starts_with(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_OBJ || args[1].type != ValType::VAL_OBJ) return false;
    if (args[0].as.obj->type != OBJ_STRING || args[1].as.obj->type != OBJ_STRING) return false;
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string prefix = static_cast<ObjString*>(args[1].as.obj)->chars;
    if (prefix.length() > s.length()) return false;
    return s.compare(0, prefix.length(), prefix) == 0;
}

SapphireValue native_string_ends_with(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_OBJ || args[1].type != ValType::VAL_OBJ) return false;
    if (args[0].as.obj->type != OBJ_STRING || args[1].as.obj->type != OBJ_STRING) return false;
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string suffix = static_cast<ObjString*>(args[1].as.obj)->chars;
    if (suffix.length() > s.length()) return false;
    return s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0;
}

SapphireValue native_string_index_of(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_OBJ || args[1].type != ValType::VAL_OBJ) return -1.0;
    if (args[0].as.obj->type != OBJ_STRING || args[1].as.obj->type != OBJ_STRING) return -1.0;
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string sub = static_cast<ObjString*>(args[1].as.obj)->chars;
    size_t pos = s.find(sub);
    if (pos == std::string::npos) return -1.0;
    return (double)pos;
}

SapphireValue native_string_last_index_of(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_OBJ || args[1].type != ValType::VAL_OBJ) return -1.0;
    if (args[0].as.obj->type != OBJ_STRING || args[1].as.obj->type != OBJ_STRING) return -1.0;
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string sub = static_cast<ObjString*>(args[1].as.obj)->chars;
    size_t pos = s.rfind(sub);
    if (pos == std::string::npos) return -1.0;
    return (double)pos;
}

SapphireValue native_string_char_at(int arg_count, SapphireValue* args) {
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

SapphireValue native_string_length(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return SapphireValue(0.0);
    }
    ObjString* str_obj = static_cast<ObjString*>(args[0].as.obj);
    return SapphireValue((double)str_obj->chars.length());
}

SapphireValue native_string_substring(int arg_count, SapphireValue* args) {
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

SapphireValue native_string_split(int arg_count, SapphireValue* args) {
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

SapphireValue native_string_replace(int arg_count, SapphireValue* args) {
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

SapphireValue native_string_to_upper(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return new_string(g_current_vm, s);
}

SapphireValue native_string_to_lower(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return new_string(g_current_vm, s);
}

SapphireValue native_string_trim(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    return new_string(g_current_vm, s);
}

SapphireValue native_string_contains(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return SapphireValue(false);
    std::string s = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string search = static_cast<ObjString*>(args[1].as.obj)->chars;
    return SapphireValue(s.find(search) != std::string::npos);
}

SapphireValue native_string_to_double(int arg_count, SapphireValue* args) {
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

