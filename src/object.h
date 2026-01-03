#ifndef SAPPHIRE_OBJECT_H
#define SAPPHIRE_OBJECT_H

#include "chunk.h"
#include "value.h"
#include "tokens.h"
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

// Forward declarations
struct ObjBoundMethod;
struct ObjFunction;
struct ObjString;
struct ObjClosure;
class VM; // Forward declaration, fancy nomination HAHAHAHAHHA

// Forward declaration para o tipo de função nativa, que usa SapphireValue
struct SapphireValue;
using NativeFn = std::function<SapphireValue(int arg_count, SapphireValue* args)>;

// Enum para identificar o tipo de objeto em tempo de execução
enum ObjType {
    OBJ_CLASS,
    OBJ_BOUND_METHOD,
    OBJ_INSTANCE,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
};

// A struct base para todos os objetos gerenciados no "heap" pela VM
struct Obj {
    ObjType type;
    bool is_marked; // Flag para o GC
    Obj* next;      // Ponteiro para o próximo objeto na lista de alocados
};

#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocate_object(vm, sizeof(type), objectType)

struct ObjClass : Obj {
    ObjString* name;
    std::unordered_map<std::string, ObjClosure*> methods;
};

// Struct para representar uma instância de uma classe
struct ObjInstance : Obj {
    ObjClass* klass; // A que classe esta instância pertence. "klass".. anlafabeto hahaha
    std::unordered_map<std::string, SapphireValue> fields;
};

struct ObjClosure : Obj {
    ObjFunction* function;
};

// Struct para armazenar strings de forma eficiente
struct ObjString : Obj {
    std::string chars;
};

// Struct para representar nossas funções compiladas
struct ObjFunction : Obj {
    int arity = 0;
    Chunk chunk;
    ObjString* name = nullptr;
};

struct ObjBoundMethod : Obj {
    SapphireValue receiver; // A instância ('this')
    ObjClosure* method;     // A closure do método
};

// Struct para "embrulhar" nossas funções C++ nativas
struct ObjNative : Obj {
    NativeFn function;
    ObjString* name;
};

// Funções "fábrica" para criar novos objetos (agora recebem VM*)
ObjBoundMethod* new_bound_method(VM* vm, SapphireValue receiver, ObjClosure* method);
ObjFunction* new_function(VM* vm);
ObjNative* new_native(VM* vm, NativeFn function);
ObjString* new_string(VM* vm, const std::string& chars);
ObjClass* new_class(VM* vm, ObjString* name);
ObjInstance* new_instance(VM* vm, ObjClass* klass);
ObjClosure* new_closure(VM* vm, ObjFunction* function);

// Declaração da função que imprime objetos (será implementada em object.cpp)
void print_object(const SapphireValue& value);

// Função auxiliar para verificar o tipo de um Obj* em tempo de execução
static inline bool is_obj_type(const SapphireValue& value, ObjType type) {
    return std::holds_alternative<Obj*>(value._value) && std::get<Obj*>(value._value)->type == type;
}
void free_object(Obj* object);

#endif //SAPPHIRE_OBJECT_H
