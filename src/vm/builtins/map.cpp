#include "builtins.h"
#include "../object.h"
#include "../value.h"

SapphireValue native_map_keys(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_MAP)) return SapphireValue();
    ObjMap* map_obj = static_cast<ObjMap*>(args[0].as.obj);
    ObjArray* keys_array = new_array(g_current_vm);
    for (const auto& pair : map_obj->items) {
        keys_array->elements.push_back(SapphireValue(new_string(g_current_vm, pair.first)));
    }
    return SapphireValue(keys_array);
}

SapphireValue native_map_values(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_MAP)) return SapphireValue();
    ObjMap* map_obj = static_cast<ObjMap*>(args[0].as.obj);
    ObjArray* vals_array = new_array(g_current_vm);
    for (const auto& pair : map_obj->items) {
        vals_array->elements.push_back(pair.second);
    }
    return SapphireValue(vals_array);
}

SapphireValue native_map_has(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_MAP) || !is_obj_type(args[1], OBJ_STRING)) return false;
    ObjMap* map_obj = static_cast<ObjMap*>(args[0].as.obj);
    std::string key = static_cast<ObjString*>(args[1].as.obj)->chars;
    return map_obj->items.find(key) != map_obj->items.end();
}

SapphireValue native_map_remove(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_MAP) || !is_obj_type(args[1], OBJ_STRING)) return false;
    ObjMap* map_obj = static_cast<ObjMap*>(args[0].as.obj);
    std::string key = static_cast<ObjString*>(args[1].as.obj)->chars;
    auto it = map_obj->items.find(key);
    if (it != map_obj->items.end()) {
        map_obj->items.erase(it);
        return true;
    }
    return false;
}

