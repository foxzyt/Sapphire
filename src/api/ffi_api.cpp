#include "ffi_api.h"
#include "../vm/vm.h"
#include "../vm/object.h"
#include "../vm/value.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <map>
#include <string>
#include <iostream>
#include <vector>
#include <cstring>
#include <ffi.h>

#ifdef _WIN32
static std::map<int, HMODULE> loaded_libs;
#else
static std::map<int, void*> loaded_libs;
#endif
static int next_lib_id = 1;

static SapphireValue ffi_api_load(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return SapphireValue(-1.0);
    std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
    
#ifdef _WIN32
    HMODULE handle = LoadLibrary(path.c_str());
    if (!handle) {
        std::cerr << "FFI Error: Could not load " << path << std::endl;
        return SapphireValue(-1.0);
    }
#else
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "FFI Error: " << dlerror() << std::endl;
        return SapphireValue(-1.0);
    }
#endif

    int id = next_lib_id++;
    loaded_libs[id] = handle;
    return SapphireValue((double)id);
}

static SapphireValue ffi_api_close(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return false;
    
    int lib_id = (int)args[0].as.number;
    auto it = loaded_libs.find(lib_id);
    if (it != loaded_libs.end()) {
#ifdef _WIN32
        FreeLibrary(it->second);
#else
        dlclose(it->second);
#endif
        loaded_libs.erase(it);
        return true;
    }
    return false;
}

static ffi_type* get_ffi_type(const std::string& type_str) {
    if (type_str == "void") return &ffi_type_void;
    if (type_str == "uint8") return &ffi_type_uint8;
    if (type_str == "sint8") return &ffi_type_sint8;
    if (type_str == "uint16") return &ffi_type_uint16;
    if (type_str == "sint16") return &ffi_type_sint16;
    if (type_str == "uint32") return &ffi_type_uint32;
    if (type_str == "sint32" || type_str == "int") return &ffi_type_sint32;
    if (type_str == "uint64") return &ffi_type_uint64;
    if (type_str == "sint64") return &ffi_type_sint64;
    if (type_str == "float") return &ffi_type_float;
    if (type_str == "double") return &ffi_type_double;
    if (type_str == "pointer" || type_str == "string") return &ffi_type_pointer;
    return &ffi_type_void;
}

static SapphireValue ffi_api_call(int arg_count, SapphireValue* args) {
    if (arg_count != 5 || args[0].type != ValType::VAL_NUMBER || 
        !is_obj_type(args[1], OBJ_STRING) || !is_obj_type(args[2], OBJ_STRING) ||
        !is_obj_type(args[3], OBJ_ARRAY) || !is_obj_type(args[4], OBJ_ARRAY)) {
        std::cerr << "FFI Error: Invalid arguments for ffiCall. Expected (lib_id, func_name, return_type, [arg_types], [args])" << std::endl;
        return SapphireValue();
    }
    
    int lib_id = (int)args[0].as.number;
    if (loaded_libs.find(lib_id) == loaded_libs.end()) {
        std::cerr << "FFI Error: Invalid library ID" << std::endl;
        return SapphireValue();
    }
    
    std::string func_name = static_cast<ObjString*>(args[1].as.obj)->chars;
    std::string ret_type_str = static_cast<ObjString*>(args[2].as.obj)->chars;
    ObjArray* arg_types_arr = static_cast<ObjArray*>(args[3].as.obj);
    ObjArray* args_arr = static_cast<ObjArray*>(args[4].as.obj);
    
    if (arg_types_arr->elements.size() != args_arr->elements.size()) {
        std::cerr << "FFI Error: arg_types and args size mismatch" << std::endl;
        return SapphireValue();
    }

#ifdef _WIN32
    HMODULE handle = loaded_libs[lib_id];
    FARPROC func = GetProcAddress(handle, func_name.c_str());
#else
    void* handle = loaded_libs[lib_id];
    void* func = dlsym(handle, func_name.c_str());
#endif

    if (!func) {
        std::cerr << "FFI Error: function " << func_name << " not found" << std::endl;
        return SapphireValue();
    }

    size_t num_args = arg_types_arr->elements.size();
    std::vector<ffi_type*> arg_types(num_args);
    std::vector<void*> arg_values(num_args);
    std::vector<uint64_t> raw_values(num_args, 0); // Alocação de 64 bits para cobrir ponteiros e doubles

    for (size_t i = 0; i < num_args; ++i) {
        if (!is_obj_type(arg_types_arr->elements[i], OBJ_STRING)) {
            std::cerr << "FFI Error: arg_type must be a string" << std::endl;
            return SapphireValue();
        }
        std::string t_str = static_cast<ObjString*>(arg_types_arr->elements[i].as.obj)->chars;
        arg_types[i] = get_ffi_type(t_str);
        SapphireValue val = args_arr->elements[i];
        
        if (t_str == "int" || t_str == "sint32" || t_str == "uint32") {
            int32_t v = (int32_t)(val.type == ValType::VAL_NUMBER ? val.as.number : 0);
            std::memcpy(&raw_values[i], &v, sizeof(int32_t));
        } else if (t_str == "double") {
            double v = (val.type == ValType::VAL_NUMBER ? val.as.number : 0.0);
            std::memcpy(&raw_values[i], &v, sizeof(double));
        } else if (t_str == "float") {
            float v = (float)(val.type == ValType::VAL_NUMBER ? val.as.number : 0.0);
            std::memcpy(&raw_values[i], &v, sizeof(float));
        } else if (t_str == "string") {
            const char* v = is_obj_type(val, OBJ_STRING) ? static_cast<ObjString*>(val.as.obj)->chars.c_str() : "";
            std::memcpy(&raw_values[i], &v, sizeof(const char*));
        } else if (t_str == "pointer") {
            void* ptr = nullptr;
            if (val.type == ValType::VAL_NUMBER) {
                ptr = (void*)(uintptr_t)val.as.number;
            } else if (is_obj_type(val, OBJ_STRING)) {
                ptr = (void*)static_cast<ObjString*>(val.as.obj)->chars.c_str();
            }
            std::memcpy(&raw_values[i], &ptr, sizeof(void*));
        } else {
            // Default num fallback
            uint64_t v = (uint64_t)(val.type == ValType::VAL_NUMBER ? val.as.number : 0);
            std::memcpy(&raw_values[i], &v, sizeof(uint64_t));
        }
        arg_values[i] = &raw_values[i];
    }

    ffi_cif cif;
    ffi_type* rtype = get_ffi_type(ret_type_str);
    
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned int)num_args, rtype, arg_types.data()) != FFI_OK) {
        std::cerr << "FFI Error: ffi_prep_cif failed" << std::endl;
        return SapphireValue();
    }

    // A return buffer large enough to hold any type returned by libffi
    uint64_t result_val = 0;
    
    try {
        ffi_call(&cif, (void (*)())func, &result_val, arg_values.data());
    } catch (...) {
        std::cerr << "FFI Error: Exception during ffi_call" << std::endl;
        return SapphireValue();
    }

    if (ret_type_str == "void") return SapphireValue();
    if (ret_type_str == "int" || ret_type_str == "sint32" || ret_type_str == "uint32") {
        int32_t res;
        std::memcpy(&res, &result_val, sizeof(int32_t));
        return SapphireValue((double)res);
    }
    if (ret_type_str == "double") {
        double res;
        std::memcpy(&res, &result_val, sizeof(double));
        return SapphireValue(res);
    }
    if (ret_type_str == "float") {
        float res;
        std::memcpy(&res, &result_val, sizeof(float));
        return SapphireValue((double)res);
    }
    if (ret_type_str == "string") {
        const char* res;
        std::memcpy(&res, &result_val, sizeof(const char*));
        if (res) return SapphireValue(new_string(g_current_vm, res));
        return SapphireValue();
    }
    if (ret_type_str == "pointer") {
        void* res;
        std::memcpy(&res, &result_val, sizeof(void*));
        return SapphireValue((double)(uintptr_t)res);
    }

    return SapphireValue();
}

void define_ffi_natives(VM* vm) {
    vm->define_native("ffiLoadLibrary", ffi_api_load);
    vm->define_native("ffiCall", ffi_api_call);
    vm->define_native("ffiCloseLibrary", ffi_api_close);
}
