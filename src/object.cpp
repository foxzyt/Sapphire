#include "object.h"
#include "tokens.h"
#include "vm.h" // Incluído para acessar a VM e registrar objetos no GC
#include <iostream>

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
        case OBJ_BOUND_METHOD:
            print_function(static_cast<ObjBoundMethod*>(obj)->method->function);
            break;
        case OBJ_NATIVE:
            std::cout << "<native fn>";
            break;
    }
}

// Template para registrar um novo objeto no GC
template <typename T>
static void register_object(VM* vm, T* object) {
    object->is_marked = false;
    object->next = vm->objects;
    vm->objects = object;
}

// Implementações das funções "fábrica"
ObjBoundMethod* new_bound_method(VM* vm, SapphireValue receiver, ObjClosure* method) {
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

ObjString* new_string(VM* vm, const std::string& chars) {
    auto* string_obj = new ObjString();
    string_obj->type = OBJ_STRING;
    string_obj->chars = chars;
    register_object(vm, string_obj);
    return string_obj;
}
