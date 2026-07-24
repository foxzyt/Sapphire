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

// Simple FFI call that takes no arguments and returns a double
// A robust FFI would use libffi to handle arbitrary arguments, but we keep it simple for now.
typedef double (*SimpleDoubleFunc)();

static SapphireValue ffi_api_call_double(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_NUMBER || !is_obj_type(args[1], OBJ_STRING)) {
        return SapphireValue();
    }
    
    int lib_id = (int)args[0].as.number;
    if (loaded_libs.find(lib_id) == loaded_libs.end()) return SapphireValue();
    
    std::string func_name = static_cast<ObjString*>(args[1].as.obj)->chars;
    
#ifdef _WIN32
    HMODULE handle = loaded_libs[lib_id];
    FARPROC func = GetProcAddress(handle, func_name.c_str());
    if (!func) return SapphireValue();
    SimpleDoubleFunc callable = (SimpleDoubleFunc)func;
#else
    void* handle = loaded_libs[lib_id];
    void* func = dlsym(handle, func_name.c_str());
    if (!func) return SapphireValue();
    SimpleDoubleFunc callable = (SimpleDoubleFunc)func;
#endif

    try {
        double result = callable();
        return SapphireValue(result);
    } catch (...) {
        return SapphireValue();
    }
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

void define_ffi_natives(VM* vm) {
    vm->define_native("ffiLoadLibrary", ffi_api_load);
    vm->define_native("ffiCallDouble", ffi_api_call_double);
    vm->define_native("ffiCloseLibrary", ffi_api_close);
}
