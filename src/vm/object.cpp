#include "object.h"
#include "tokens.h"
#include "vm.h" // Incluído para acessar a VM e registrar objetos no GC
#include <iostream>
#include <thread>
#include <cmath>

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
        case OBJ_MAP: delete static_cast<ObjMap*>(object); break;
        case OBJ_PROMISE: delete static_cast<ObjPromise*>(object); break;
        case OBJ_ARRAY: delete static_cast<ObjArray*>(object); break;
        case OBJ_LRU: delete static_cast<ObjLRU*>(object); break;
        case OBJ_FADE: delete static_cast<ObjFade*>(object); break;
        case OBJ_BIGINT: delete static_cast<ObjBigInt*>(object); break;
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
    Obj* obj = value.as.obj;
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
            if (bound->method.type == ValType::VAL_OBJ) {
                Obj* method_obj = bound->method.as.obj;
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
        case OBJ_MAP: {
            ObjMap* map_obj = static_cast<ObjMap*>(obj);
            std::cout << "{";
            bool first = true;
            for (const auto& pair : map_obj->items) {
                if (!first) std::cout << ", ";
                std::cout << "\"" << pair.first << "\": ";
                print_value(pair.second);
                first = false;
            }
            std::cout << "}";
            break;
        }
        case OBJ_PROMISE:
            std::cout << "<promise>";
            break;
        case OBJ_ARRAY: {
            ObjArray* arr = static_cast<ObjArray*>(obj);
            std::cout << "[";
            for (size_t i = 0; i < arr->elements.size(); ++i) {
                print_value(arr->elements[i]);
                if (i < arr->elements.size() - 1) std::cout << ", ";
            }
            std::cout << "]";
            break;
        }
        case OBJ_LRU: {
            ObjLRU* lru = static_cast<ObjLRU*>(obj);
            std::cout << "<lru_cache size=" << lru->items.size() << " cap=" << lru->capacity << ">";
            break;
        }
        case OBJ_BIGINT: {
            ObjBigInt* bigint = static_cast<ObjBigInt*>(obj);
            if (bigint->digits.empty()) {
                std::cout << "0";
            } else {
                if (bigint->is_negative) std::cout << "-";
                for (int i = (int)bigint->digits.size() - 1; i >= 0; --i) {
                    if (i == (int)bigint->digits.size() - 1) {
                        std::cout << bigint->digits[i];
                    } else {
                        std::string s = std::to_string(bigint->digits[i]);
                        std::cout << std::string(9 - s.length(), '0') << s;
                    }
                }
            }
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
        std::cerr << "CRITICAL ERROR: CORUNDUM VM OOM (Out of Memory) DETECTED! Aborting immediately to prevent system crash!" << std::endl;
        std::cerr << "Memory exceeded safe limit of " << vm->max_memory_limit / (1024*1024) << " MB in thread " << std::this_thread::get_id() << std::endl;
        std::cerr << "[DEBUG] bytes_allocated=" << vm->bytes_allocated << " max_memory_limit=" << vm->max_memory_limit << "\n"; exit(1); // Aborta tudo imediatamente!
    }

    if (vm->bytes_allocated > vm->next_gc_threshold) {
        vm->step_gc();
    }
}

// Implementações das funções "fábrica"
ObjBoundMethod* new_bound_method(VM* vm, SapphireValue receiver, SapphireValue method, ObjClass* defined_in_class) {
    auto* bound = new ObjBoundMethod();
    bound->type = OBJ_BOUND_METHOD;
    bound->receiver = receiver;
    bound->method = method;
    bound->defined_in_class = defined_in_class;
    register_object(vm, bound);
    return bound;
}

ObjClass* new_class(VM* vm, ObjString* name) {
    auto* klass = new ObjClass();
    klass->type = OBJ_CLASS;
    klass->name = name;
    klass->superclass = nullptr;
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

ObjMap* new_map(VM* vm) {
    auto* map_obj = new ObjMap();
    map_obj->type = OBJ_MAP;
    register_object(vm, map_obj);
    return map_obj;
}

ObjPromise* new_promise(VM* vm) {
    auto* promise_obj = new ObjPromise();
    promise_obj->type = OBJ_PROMISE;
    promise_obj->state = PromiseState::PENDING;
    register_object(vm, promise_obj);
    return promise_obj;
}

ObjArray* new_array(VM* vm) {
    auto* arr = new ObjArray();
    arr->type = OBJ_ARRAY;
    register_object(vm, arr);
    return arr;
}

ObjLRU* new_lru(VM* vm, int capacity) {
    auto* lru = new ObjLRU();
    lru->type = OBJ_LRU;
    lru->capacity = capacity;
    register_object(vm, lru);
    return lru;
}

ObjFade* new_fade(VM* vm, SapphireValue value, double duration_ms, const std::string& curve_type) {
    ObjFade* fade = new ObjFade();
    fade->type = OBJ_FADE;
    fade->value = value;
    fade->duration_ms = duration_ms;
    fade->curve_type = curve_type;
    register_object(vm, fade);
    return fade;
}

ObjBigInt* new_bigint(VM* vm) {
    ObjBigInt* bigint = new ObjBigInt();
    bigint->type = OBJ_BIGINT;
    bigint->is_negative = false;
    register_object(vm, bigint);
    return bigint;
}

// --- BigInt Math ---
const uint32_t BIGINT_BASE = 1000000000; // 10^9

void trim_bigint(ObjBigInt* b) {
    while (b->digits.size() > 0 && b->digits.back() == 0) {
        b->digits.pop_back();
    }
    if (b->digits.empty()) b->is_negative = false;
}

ObjBigInt* new_bigint_from_double(VM* vm, double value) {
    ObjBigInt* bigint = new_bigint(vm);
    if (value < 0) {
        bigint->is_negative = true;
        value = -value;
    }
    if (value < 1.0) return bigint;
    
    while (value >= 1.0) {
        double rem = std::fmod(value, (double)BIGINT_BASE);
        bigint->digits.push_back(static_cast<uint32_t>(rem));
        value = std::floor(value / (double)BIGINT_BASE);
    }
    return bigint;
}

int cmp_bigint_abs(ObjBigInt* a, ObjBigInt* b) {
    if (a->digits.size() != b->digits.size()) {
        return a->digits.size() > b->digits.size() ? 1 : -1;
    }
    for (int i = (int)a->digits.size() - 1; i >= 0; --i) {
        if (a->digits[i] != b->digits[i]) {
            return a->digits[i] > b->digits[i] ? 1 : -1;
        }
    }
    return 0;
}

int cmp_bigint(ObjBigInt* a, ObjBigInt* b) {
    if (a->is_negative != b->is_negative) {
        return a->is_negative ? -1 : 1;
    }
    int abs_cmp = cmp_bigint_abs(a, b);
    return a->is_negative ? -abs_cmp : abs_cmp;
}

ObjBigInt* add_bigint_abs(VM* vm, ObjBigInt* a, ObjBigInt* b) {
    ObjBigInt* res = new_bigint(vm);
    uint32_t carry = 0;
    size_t n = std::max(a->digits.size(), b->digits.size());
    for (size_t i = 0; i < n || carry; ++i) {
        uint64_t sum = carry;
        if (i < a->digits.size()) sum += a->digits[i];
        if (i < b->digits.size()) sum += b->digits[i];
        res->digits.push_back(sum % BIGINT_BASE);
        carry = sum / BIGINT_BASE;
    }
    return res;
}

ObjBigInt* sub_bigint_abs(VM* vm, ObjBigInt* a, ObjBigInt* b) { // Assumes a >= b
    ObjBigInt* res = new_bigint(vm);
    uint32_t borrow = 0;
    for (size_t i = 0; i < a->digits.size(); ++i) {
        int64_t diff = a->digits[i] - borrow;
        if (i < b->digits.size()) diff -= b->digits[i];
        if (diff < 0) {
            diff += BIGINT_BASE;
            borrow = 1;
        } else {
            borrow = 0;
        }
        res->digits.push_back((uint32_t)diff);
    }
    trim_bigint(res);
    return res;
}

ObjBigInt* add_bigint(VM* vm, ObjBigInt* a, ObjBigInt* b) {
    if (a->is_negative == b->is_negative) {
        ObjBigInt* res = add_bigint_abs(vm, a, b);
        res->is_negative = a->is_negative;
        return res;
    }
    if (cmp_bigint_abs(a, b) >= 0) {
        ObjBigInt* res = sub_bigint_abs(vm, a, b);
        res->is_negative = a->is_negative;
        return res;
    }
    ObjBigInt* res = sub_bigint_abs(vm, b, a);
    res->is_negative = b->is_negative;
    return res;
}

ObjBigInt* sub_bigint(VM* vm, ObjBigInt* a, ObjBigInt* b) {
    b->is_negative = !b->is_negative;
    ObjBigInt* res = add_bigint(vm, a, b);
    b->is_negative = !b->is_negative;
    return res;
}

ObjBigInt* mul_bigint(VM* vm, ObjBigInt* a, ObjBigInt* b) {
    ObjBigInt* res = new_bigint(vm);
    if (a->digits.empty() || b->digits.empty()) return res;
    
    res->digits.resize(a->digits.size() + b->digits.size(), 0);
    for (size_t i = 0; i < a->digits.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b->digits.size() || carry; ++j) {
            uint64_t cur = res->digits[i + j] + 
                           (uint64_t)a->digits[i] * (j < b->digits.size() ? b->digits[j] : 0) + carry;
            res->digits[i + j] = cur % BIGINT_BASE;
            carry = cur / BIGINT_BASE;
        }
    }
    res->is_negative = (a->is_negative != b->is_negative);
    trim_bigint(res);
    return res;
}
