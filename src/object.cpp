#include "object.h"
#include <iostream>

// NOTA IMPORTANTE SOBRE MEMÓRIA:
// Estamos usando 'new' para alocar memória para nossos objetos. Em uma
// linguagem de produção, precisaríamos de um "Coletor de Lixo" (Garbage Collector)
// para automaticamente liberar essa memória quando os objetos não forem mais usados.
// Por enquanto, nosso programa irá vazar memória, mas isso é aceitável para
// o nosso objetivo atual de fazer o compilador e a VM funcionarem.

// Função auxiliar para imprimir um objeto de função
static void print_function(ObjFunction* function) {
    if (function->name == nullptr) {
        // O corpo principal do script é uma função sem nome.
        std::cout << "<script>";
        return;
    }
    std::cout << "<fn " << function->name->chars << ">";
}

// Função principal que sabe como imprimir cada tipo de objeto
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
            // Imprimir uma closure é o mesmo que imprimir a função que ela envolve
            print_function(static_cast<ObjClosure*>(obj)->function);
            break;
        case OBJ_FUNCTION:
            print_function(static_cast<ObjFunction*>(obj));
            break;
        case OBJ_BOUND_METHOD:
            // Imprime como a função original para sabermos qual é
            print_function(static_cast<ObjBoundMethod*>(obj)->method->function);
            break;
        case OBJ_NATIVE:
            std::cout << "<native fn>";
            break;
    }
}


// Implementações das funções "fábrica"

ObjBoundMethod* new_bound_method(SapphireValue receiver, ObjClosure* method) {
    auto* bound = new ObjBoundMethod();
    bound->type = OBJ_BOUND_METHOD;
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}
ObjClass* new_class(ObjString* name) {
    auto* klass = new ObjClass();
    klass->type = OBJ_CLASS;
    klass->name = name;
    return klass;
}

ObjInstance* new_instance(ObjClass* klass) {
    auto* instance = new ObjInstance();
    instance->type = OBJ_INSTANCE;
    instance->klass = klass;
    return instance;
}

ObjFunction* new_function() {
    auto* function = new ObjFunction();
    function->type = OBJ_FUNCTION;
    return function;
}

ObjNative* new_native(NativeFn function) {
    auto* native = new ObjNative();
    native->type = OBJ_NATIVE;
    native->function = function;
    return native;
}

ObjClosure* new_closure(ObjFunction* function) {
    auto* closure = new ObjClosure();
    closure->type = OBJ_CLOSURE;
    closure->function = function;
    return closure;
}

ObjString* new_string(const std::string& chars) {
    auto* string_obj = new ObjString();
    string_obj->type = OBJ_STRING;
    string_obj->chars = chars;
    return string_obj;
}