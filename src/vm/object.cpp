#include "object.h"
#include "tokens.h"
#include "vm.h" // Incluído para acessar a VM e registrar objetos no GC
#include <iostream>
#include <thread>

using enum TokenType;

// Função para liberar a memória de um único objeto, chamada pelo GC
void free_object(Obj* object) {
    // std::cout << "GC: Liberando objeto tipo " << object->type << std::endl;

    switch (object->type) {
        case OBJ_STRING:   delete static_cast<ObjString*>(object); break;
        case OBJ_FUNCTION: delete static_cast<ObjFunction*>(object); break;
        case OBJ_NATIVE:   delete static_cast<ObjNative*>(object); break;
        case OBJ_CLOSURE:  delete static_cast<ObjClosure*>(object); break;
        case OBJ_CLASS:    delete static_cast<ObjClass*>(object); break;
        case OBJ_INSTANCE: delete static_cast<ObjInstance*>(object); break;
        case OBJ_BOUND_METHOD: delete static_cast<ObjBoundMethod*>(object); break;
        case OBJ_NAMED_ARG: delete static_cast<ObjNamedArg*>(object); break;
    }
}


static void print_function(ObjFunction* function) {
    if (function->name == nullptr) {
        std::cout << "<script>";
        return;
    }
    std::cout << "<fn " << function->name->chars << ">";
}

void print_object(const SapphireValue& value) {
    Obj* obj = std::get<Obj*>(value._value);
    switch (obj->type) {
        case OBJ_STRING:
            std::cout << static_cast<ObjString*>(obj)->chars;
            break;
        case OBJ_CLASS:
            std::cout << static_cast<ObjClass*>(obj)->name->chars;
            break;
        case OBJ_INSTANCE:
            std::cout << static_cast<ObjInstance*>(obj)->klass->name->chars << " instance";
            break;
        case OBJ_CLOSURE:
            print_function(static_cast<ObjClosure*>(obj)->function);
            break;
        case OBJ_FUNCTION:
            print_function(static_cast<ObjFunction*>(obj));
            break;
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = static_cast<ObjBoundMethod*>(obj);
            if (std::holds_alternative<Obj*>(bound->method._value)) {
                Obj* method_obj = std::get<Obj*>(bound->method._value);
                if (method_obj->type == OBJ_CLOSURE) {
                    print_function(static_cast<ObjClosure*>(method_obj)->function);
                } else if (method_obj->type == OBJ_NATIVE) {
                    std::cout << "<native method>";
                }
            }
            break;
        }
        case OBJ_NATIVE:
            std::cout << "<native fn>";
            break;
        case OBJ_NAMED_ARG: {
            ObjNamedArg* arg = static_cast<ObjNamedArg*>(obj);
            std::cout << arg->name->chars << "=";
            print_value(arg->value);
            break;
        }
    }
}

// Template para registrar um novo objeto no GC
template <typename T>
static void register_object(VM* vm, T* object) {
    object->is_marked = (vm->gc_state == VM::GCState::GC_TRACE || vm->gc_state == VM::GCState::GC_SWEEP);
    object->next = vm->objects;
    vm->objects = object;

    vm->bytes_allocated += sizeof(T);
    
    // NOVO: Hard Limit para evitar BSOD e Memory Leaks Incontroláveis!
    if (vm->bytes_allocated > vm->max_memory_limit) {
        std::cerr << "CRITICAL ERROR: SAPPHIRE VM OOM (Out of Memory) DETECTED! Aborting immediately to prevent system crash!" << std::endl;
        std::cerr << "Memory exceeded safe limit of " << vm->max_memory_limit / (1024*1024) << " MB in thread " << std::this_thread::get_id() << std::endl;
        exit(1); // Aborta tudo imediatamente!
    }

    if (vm->bytes_allocated > vm->next_gc_threshold) {
        vm->step_gc();
    }
}

// Implementações das funções "fábrica"
ObjBoundMethod* new_bound_method(VM* vm, SapphireValue receiver, SapphireValue method) {
    auto* bound = new ObjBoundMethod();
    bound->type = OBJ_BOUND_METHOD;
    bound->receiver = receiver;
    bound->method = method;
    register_object(vm, bound);
    return bound;
}

ObjClass* new_class(VM* vm, ObjString* name) {
    auto* klass = new ObjClass();
    klass->type = OBJ_CLASS;
    klass->name = name;
    register_object(vm, klass);
    return klass;
}

ObjInstance* new_instance(VM* vm, ObjClass* klass) {
    auto* instance = new ObjInstance();
    instance->type = OBJ_INSTANCE;
    instance->klass = klass;
    register_object(vm, instance);
    return instance;
}

ObjFunction* new_function(VM* vm) {
    auto* function = new ObjFunction();
    function->type = OBJ_FUNCTION;
    register_object(vm, function);
    return function;
}

ObjNative* new_native(VM* vm, NativeFn function) {
    auto* native = new ObjNative();
    native->type = OBJ_NATIVE;
    native->function = function;
    register_object(vm, native);
    return native;
}

ObjClosure* new_closure(VM* vm, ObjFunction* function) {
    auto* closure = new ObjClosure();
    closure->type = OBJ_CLOSURE;
    closure->function = function;
    register_object(vm, closure);
    return closure;
}

ObjNamedArg* new_named_arg(VM* vm, ObjString* name, SapphireValue value) {
    auto* arg = new ObjNamedArg();
    arg->type = OBJ_NAMED_ARG;
    arg->name = name;
    arg->value = value;
    register_object(vm, arg);
    return arg;
}

ObjString* new_string(VM* vm, const std::string& chars) {
    auto* string_obj = new ObjString();
    string_obj->type = OBJ_STRING;
    string_obj->chars = chars;
    vm->bytes_allocated += chars.capacity(); // Account for inner string buffer
    register_object(vm, string_obj);
    return string_obj;
}
