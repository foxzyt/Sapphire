#include "builtins.h"
#include "../object.h"
#include "../value.h"

SapphireValue native_list_util_reverse(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_ARRAY) return SapphireValue();
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    std::reverse(list_obj->elements.begin(), list_obj->elements.end());
    return args[0];
}

SapphireValue native_list_util_clear(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_ARRAY) return SapphireValue();
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    list_obj->elements.clear();
    return args[0];
}

SapphireValue native_list_util_index_of(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_ARRAY) return -1.0;
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    auto it = std::find_if(list_obj->elements.begin(), list_obj->elements.end(),
                           [&](const SapphireValue& v) { return values_equal(v, args[1]); });
    if (it != list_obj->elements.end()) {
        return (double)std::distance(list_obj->elements.begin(), it);
    }
    return -1.0;
}

SapphireValue native_list_util_join(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || arg_count > 2 || args[0].type != ValType::VAL_OBJ || args[0].as.obj->type != OBJ_ARRAY) return SapphireValue();
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    std::string sep = "";
    if (arg_count == 2 && args[1].type == ValType::VAL_OBJ && args[1].as.obj->type == OBJ_STRING) {
        sep = static_cast<ObjString*>(args[1].as.obj)->chars;
    }
    std::string res;
    for (size_t i = 0; i < list_obj->elements.size(); ++i) {
        if (i > 0) res += sep;
        SapphireValue str_val = native_value_to_string(1, &list_obj->elements[i]);
        if (str_val.type == ValType::VAL_OBJ && str_val.as.obj->type == OBJ_STRING) {
            res += static_cast<ObjString*>(str_val.as.obj)->chars;
        }
    }
    return SapphireValue(new_string(g_current_vm, res));
}

SapphireValue native_list_util_create(int arg_count, SapphireValue* args) {
    if (arg_count != 0) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.create() expects 0 arguments." << std::endl;
        }
        return {};
    }
    return new_array(g_current_vm);
}

SapphireValue native_list_util_append(int arg_count, SapphireValue* args) {
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

SapphireValue native_list_util_get(int arg_count, SapphireValue* args) {
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

SapphireValue native_list_util_set(int arg_count, SapphireValue* args) {
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

SapphireValue native_list_util_length(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_ARRAY)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.length() expects 1 list argument." << std::endl;
        }
        return 0.0;
    }
    auto list_obj = static_cast<ObjArray*>(args[0].as.obj);
    return (double)list_obj->elements.size();
}

SapphireValue native_list_util_remove_at(int arg_count, SapphireValue* args) {
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

SapphireValue native_list_util_contains(int arg_count, SapphireValue* args) {
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

