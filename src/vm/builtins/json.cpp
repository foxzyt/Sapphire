#include "builtins.h"
#include "../object.h"
#include "../value.h"
#include "nlohmann/json.hpp"

nlohmann::json convertSapphireToJson(SapphireValue val) {
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

SapphireValue convertJsonToSapphire(VM* vm, const nlohmann::json& j);
nlohmann::json convertSapphireToJson(SapphireValue value);
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

SapphireValue convertJsonToSapphire(VM* vm, const nlohmann::json& j) {
    if (j.is_object()) return convertJsonObjectToSapphireMap(vm, j);
    if (j.is_array()) return convertJsonArrayToSapphireArray(vm, j);
    if (j.is_string()) return new_string(vm, j.get<std::string>());
    if (j.is_number()) return j.get<double>();
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_null()) return {};
    return {};
}




SapphireValue native_json_parse(int arg_count, SapphireValue* args) {
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


SapphireValue native_json_stringify(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: JSON.stringify() expects 1 argument." << std::endl;
        }
        return {};
    }
    nlohmann::json j = convertSapphireToJson(args[0]);
    return new_string(g_current_vm, j.dump());
}
