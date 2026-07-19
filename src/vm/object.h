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
struct ObjPromise;
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
    OBJ_NAMED_ARG,
    OBJ_MAP,
    OBJ_PROMISE,
    OBJ_ARRAY,
    OBJ_LRU,
    OBJ_FADE,
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
    ObjClass* superclass; // Added for inheritance
    std::unordered_map<std::string, SapphireValue> methods;
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

struct ObjFunction : Obj {
    int arity = 0;
    Chunk chunk;
    ObjString* name = nullptr;
    ObjClass* owner_class = nullptr; // The class that owns this method (if any)
    bool is_async = false;
};

struct ObjBoundMethod : Obj {
    SapphireValue receiver; // A instância ('this')
    SapphireValue method;   // A closure ou native do método
    ObjClass* defined_in_class; // The class where this method was defined
};

// Struct para "embrulhar" nossas funções C++ nativas
struct ObjNative : Obj {
    NativeFn function;
    ObjString* name;
};

// Struct para representar um argumento nomeado (kwarg)
struct ObjNamedArg : Obj {
    ObjString* name;
    SapphireValue value;
};

struct ObjMap : Obj {
    std::unordered_map<std::string, SapphireValue> items;
};

struct ObjArray : Obj {
    std::vector<SapphireValue> elements;
};

struct ObjLRU : Obj {
    int capacity;
    std::list<std::string> order;
    std::unordered_map<std::string, SapphireValue> items;
};

struct ObjFade : Obj {
    SapphireValue value;
    double duration_ms;
    std::string curve_type;
    uint64_t created_at_ms;
};

// Funções "fábrica" para criar novos objetos (agora recebem VM*)
ObjBoundMethod* new_bound_method(VM* vm, SapphireValue receiver, SapphireValue method, ObjClass* defined_in_class);
ObjFunction* new_function(VM* vm);
ObjNative* new_native(VM* vm, NativeFn function);
ObjString* new_string(VM* vm, const std::string& chars);
ObjClass* new_class(VM* vm, ObjString* name);
ObjInstance* new_instance(VM* vm, ObjClass* klass);
ObjClosure* new_closure(VM* vm, ObjFunction* function);
ObjNamedArg* new_named_arg(VM* vm, ObjString* name, SapphireValue value);
ObjMap* new_map(VM* vm);
ObjPromise* new_promise(VM* vm);
ObjArray* new_array(VM* vm);
ObjLRU* new_lru(VM* vm, int capacity);
ObjFade* new_fade(VM* vm, SapphireValue value, double duration_ms, const std::string& curve_type);

// Declaração da função que imprime objetos (será implementada em object.cpp)
void print_object(const SapphireValue& value);

// Função auxiliar para verificar o tipo de um Obj* em tempo de execução
static inline bool is_obj_type(const SapphireValue& value, ObjType type) {
    return value.type == ValType::VAL_OBJ && value.as.obj->type == type;
}
void free_object(Obj* object);

#endif //SAPPHIRE_OBJECT_H
