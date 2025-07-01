#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "debug.h"
#include "value.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

#define BINARY_OP(value_type, op) \
    do { \
        if (!std::holds_alternative<double>(peek(0)._value) || !std::holds_alternative<double>(peek(1)._value)) { \
            std::cerr << "\n[Runtime Error] Binary operator expected two numbers but got: " \
                      << get_value_type_name(peek(1)) << " and " << get_value_type_name(peek(0)) << "\n"; \
            return false; \
        } \
        double b = std::get<double>(pop()._value); \
        double a = std::get<double>(pop()._value); \
        push(value_type(a op b)); \
    } while (false)

// --- Função Nativa ---
static const auto clock_start_time = std::chrono::high_resolution_clock::now();
static SapphireValue clock_native(int arg_count, SapphireValue* args) {
    if (arg_count != 0) return {}; 
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = now - clock_start_time;
    return diff.count();
}



//FUNÇÃO NATIVA PARA LEITURA
static SapphireValue io_readline_native(int arg_count, SapphireValue* args) {
    // 1. Verifica se a função foi chamada com o número correto de argumentos (zero)
    if (arg_count != 0) {
        // No futuro, podemos implementar um sistema de erro mais robusto aqui
        std::cerr << "Runtime Error: IO.readLine() expects 0 arguments." << std::endl;
        return {}; // Retorna nil em caso de erro
    }

    // 2. Lê uma linha inteira do terminal
    std::string line;
    std::getline(std::cin, line);

    // 3. Cria um objeto de string da Sapphire e o retorna
    return new_string(line);
}
static SapphireValue native_math_sqrt(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        std::cerr << "Runtime Error: sqrt() expects 1 argument." << std::endl;
        return {}; 
    }
    if (!std::holds_alternative<double>(args[0]._value)) {
        std::cerr << "Runtime Error: Argument for sqrt() must be a number." << std::endl;
        return {};
    }
    double number = std::get<double>(args[0]._value);
    return sqrt(number);
}

// --- Construtor e Funções da VM ---
VM::VM() {
    frame_count = 0;
    stack_top = stack;
    
    // --- Funções Nativas Globais ---
    define_native("clock", clock_native);

    // --- Biblioteca Nativa de IO ---
    // 1. Criamos um objeto de classe genérico para nossa biblioteca.
    ObjClass* io_class = new_class(new_string("IO")); 
    // 2. Criamos uma instância que será nosso objeto 'IO'.
    ObjInstance* io_object = new_instance(io_class);

    // 3. Adicionamos nossas funções nativas como campos neste objeto.
    //    O valor do campo é um objeto de função nativa.
    io_object->fields["readLine"] = new_native(io_readline_native);
    
    // 4. Finalmente, registramos o objeto 'io_object' como uma variável global chamada "IO".
    globals["IO"] = io_object;

    ObjClass* math_class = new_class(new_string("Math"));
    ObjInstance* math_object = new_instance(math_class);
    math_object->fields["sqrt"] = new_native(native_math_sqrt);
    globals["Math"] = math_object;
}
void VM::define_native(const std::string& name, NativeFn function) {
    globals[name] = new_native(function);
}

void VM::push(const SapphireValue& value) {
    *stack_top = value;
    stack_top++;
}

SapphireValue VM::pop() {
    // Pega o frame de chamada atual
    CallFrame* frame = &frames[frame_count - 1];

    // Se o topo da pilha já está no início dos slots deste frame,
    // significa que não há mais nada para dar pop neste escopo.
    // Tentar dar pop de novo causaria um underflow.
    if (stack_top == frame->slots) {
        std::cerr << "Runtime Error: Stack underflow in frame '" 
                  << (frame->function->name != nullptr ? frame->function->name->chars : "<script>")
                  << "'." << std::endl;
        
        exit(70); // Código de erro padrão para erro interno de software
    }

    // Se a pilha não está vazia, a operação continua normalmente.
    stack_top--;
    return *stack_top;
}

SapphireValue& VM::peek(int distance) {
    return stack_top[-1 - distance];
}

bool VM::call(ObjFunction* function, int arg_count) {
    if (arg_count != function->arity) {
        std::cerr << "Erro de Runtime: Esperava " << function->arity << " argumentos mas recebeu " << arg_count << "." << std::endl;
        return false;
    }
    if (frame_count == FRAMES_MAX) {
        std::cerr << "Erro de Runtime: Estouro da pilha de chamadas (stack overflow)." << std::endl;
        return false;
    }

    CallFrame* frame = &frames[frame_count++];
    frame->function = function;
    frame->ip = &function->chunk.code[0];
    frame->slots = stack_top - arg_count - 1;
    return true;
}

bool VM::call_value(SapphireValue callee, int arg_count) {
    if (std::holds_alternative<Obj*>(callee._value)) {
        Obj* obj = std::get<Obj*>(callee._value);
        switch (obj->type) {
            case OBJ_CLASS: {
                ObjClass* klass = static_cast<ObjClass*>(obj);
                // O resultado da "chamada" de uma classe é uma nova instância.
                // Colocamos a instância no lugar da classe na pilha.
                peek(arg_count) = new_instance(klass); 
                return true;
            }
            case OBJ_CLOSURE:
                // Agora chama a função que está DENTRO da closure
                return call(static_cast<ObjClosure*>(obj)->function, arg_count);
            
            case OBJ_NATIVE: {
                NativeFn native = static_cast<ObjNative*>(obj)->function;
                SapphireValue result = native(arg_count, stack_top - arg_count);
                stack_top -= arg_count + 1;
                push(result);
                return true;
            }
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = static_cast<ObjBoundMethod*>(obj);
                // O "receiver" (a instância 'this') é colocado na base da pilha
                // para o próximo quadro de chamada, no lugar do próprio bound method.
                peek(arg_count) = bound->receiver;
                // Agora, chamamos a função real do método
                return call(bound->method->function, arg_count);
            }
            default:
                // Não é um valor chamável (ex: string)
                break;
        }
    }

      std::cerr << "\n[Runtime Error] Value is not callable (must be function, class, or bound method).\n";
      return false;

}

// --- MACRO PARA OPERAÇÕES BINÁRIAS ---
  


// --- O CORAÇÃO DA VM: O LOOP DE EXECUÇÃO ---
bool VM::run() {
    CallFrame* frame = &frames[frame_count - 1];

    for (;;) {

        #ifdef DEBUG_TRACE_EXECUTION
            debug_print_stack(this);
            disassemble_instruction(frame->function->chunk, (int)(frame->ip - &frame->function->chunk.code[0]));
        #endif

        uint8_t instruction = *frame->ip++;
        switch (instruction) {
            case OP_CONSTANT:      push(frame->function->chunk.constants[*frame->ip++]); break;
            case OP_NIL:           push({}); break;
            case OP_TRUE:          push(true); break;
            case OP_FALSE:         push(false); break;
            case OP_POP:           pop(); break;
            
            case OP_GET_LOCAL:     push(frame->slots[*frame->ip++]); break;
            case OP_SET_LOCAL:     frame->slots[*frame->ip++] = peek(0); break;

            case OP_GET_GLOBAL: {
    ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[*frame->ip++]._value));
    auto it = globals.find(name->chars);
    if (it == globals.end()) {
      std::cerr << "\n[Runtime Error] Undefined global variable: '" << name->chars << "'\n";
      return false;
    }
    push(it->second);
    break;
}
            case OP_DEFINE_GLOBAL: {
    ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[*frame->ip++]._value));
    globals[name->chars] = peek(0);
    pop();
    break;
}
            case OP_GET_PROPERTY: {
    if (!is_obj_type(peek(0), OBJ_INSTANCE)) {
      std::cerr << "\n[Runtime Error] Attempted property access on non-instance object.\n";
      return false;
    
    }
    ObjInstance* instance = static_cast<ObjInstance*>(std::get<Obj*>(peek(0)._value));
    ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[*frame->ip++]._value));

    // 1. Procura por um campo na instância.
    auto it_field = instance->fields.find(name->chars);
    if (it_field != instance->fields.end()) {
        pop(); // Remove a instância
        push(it_field->second); // Coloca o valor do campo na pilha
        break;
    }

    // 2. Se não encontrou um campo, procura por um método na classe.
    auto it_method = instance->klass->methods.find(name->chars);
    if (it_method != instance->klass->methods.end()) {
        ObjClosure* method = it_method->second;
        ObjBoundMethod* bound = new_bound_method(peek(0), method);
        pop(); // Remove a instância
        push(bound); // Coloca o bound method, pronto para ser chamado.
        break;
    }

    // 3. Se não encontrou nem campo nem método, a propriedade é indefinida.
    //    Retornamos 'nil' em vez de gerar um erro fatal.
    pop(); // Remove a instância da pilha
    push({}); // Empurra um valor nil (SapphireValue{} é nil)
    
    break;
}
case OP_SET_PROPERTY: {
    if (std::get_if<Obj*>(&peek(1)._value) == nullptr || std::get<Obj*>(peek(1)._value)->type != OBJ_INSTANCE) {

      std::cerr << "\n[Runtime Error] Only instance objects can have fields assigned.\n";
      return false;

    }
    ObjInstance* instance = static_cast<ObjInstance*>(std::get<Obj*>(peek(1)._value));
    ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[*frame->ip++]._value));
    
    // Atribui usando a string, não o ponteiro
    instance->fields[name->chars] = peek(0); 

    SapphireValue value = pop();
    pop();
    push(value);
    break;
}
            case OP_SET_GLOBAL: { 
    ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[*frame->ip++]._value));
    auto it = globals.find(name->chars);
    if (it == globals.end()) {
         std::cerr << "Runtime Error: Undefined global variable for assignment '" << name->chars << "'." << std::endl;
        return false;
    }
    it->second = peek(0);
    break;
}
            case OP_CLOSURE: { 
    ObjFunction* function = static_cast<ObjFunction*>(std::get<Obj*>(frame->function->chunk.constants[*frame->ip++]._value));
    ObjClosure* closure = new_closure(function);
    push(closure);
    break;
} 
            
            case OP_EQUAL:   { SapphireValue b = pop(); SapphireValue a = pop(); push(a._value == b._value); break; }
            case OP_GREATER:  BINARY_OP(bool, >); break;
            case OP_LESS:     BINARY_OP(bool, <); break;
            
            case OP_ADD: {
    // Verifica se os dois operandos no topo da pilha são strings
    if (is_obj_type(peek(0), OBJ_STRING) && is_obj_type(peek(1), OBJ_STRING)) {
        // 1. Faz o pop dos valores e os armazena em ponteiros.
        ObjString* b = static_cast<ObjString*>(std::get<Obj*>(pop()._value));
        ObjString* a = static_cast<ObjString*>(std::get<Obj*>(pop()._value));
        
        // 2.cria um novo objeto de string.
        push(new_string(a->chars + b->chars));

    } else if (std::holds_alternative<double>(peek(0)._value) && std::holds_alternative<double>(peek(1)._value)) {
        // A lógica para números permanece a mesma
        double b = std::get<double>(pop()._value);
        double a = std::get<double>(pop()._value);
        push(a + b);
    } else {
        // Se os tipos não corresponderem, gera um erro.
        
      std::cerr << "\n[Runtime Error] '+' operator requires two numbers or two strings.\n";
      return false;
    
    }
    break;
}
            case OP_SUBTRACT: BINARY_OP(double, -); break;
            case OP_MULTIPLY: BINARY_OP(double, *); break;
            case OP_DIVIDE:   BINARY_OP(double, /); break;
            
            case OP_NOT:      push(is_falsey(pop())); break;
            case OP_NEGATE:
                if (!std::holds_alternative<double>(peek(0)._value)) {
                   
                  std::cerr << "\n[Runtime Error] Operand for unary '-' must be a number.\n";
                  return false;
                }
                push(-std::get<double>(pop()._value));
                break;

            case OP_PRINT: {
                print_value(pop());
                std::cout << std::endl;
                break;
            }

            case OP_JUMP: {
                uint16_t offset = (frame->ip[0] << 8) | frame->ip[1];
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                if (is_falsey(peek(0))) {
                    frame->ip += offset;
                }
                break;
            }
            case OP_LOOP: {
                uint16_t offset = (frame->ip[0] << 8) | frame->ip[1];
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int arg_count = *frame->ip++;
                if (!call_value(peek(arg_count), arg_count)) {
                    return false;
                }
                frame = &frames[frame_count - 1];
                break;
            }
            case OP_RETURN: {
                SapphireValue result = pop();
                frame_count--;
                if (frame_count == 0) {
                    pop();
                    return true;
                }
                stack_top = frame->slots;
                push(result);
                frame = &frames[frame_count - 1];
                break;
            }
            case OP_BUILD_ARRAY: {
                // 1. O operando é a quantidade de elementos no array.
                uint8_t element_count = *frame->ip++;

                // 2. Cria o nosso objeto de array.
                auto array_obj = std::make_shared<SapphireArray>();

                // 3. Pega os 'element_count' elementos do topo da pilha e os adiciona ao array.
                // Usamos peek para não bagunçar a pilha antes da hora.
                for (int i = 0; i < element_count; i++) {
                    array_obj->elements.push_back(peek(element_count - 1 - i));
                }

                // 4. Agora, remove os elementos originais da pilha.
                for (int i = 0; i < element_count; i++) {
                    pop();
                }
                
                // 5. Coloca o novo objeto de array na pilha.
                push(array_obj);
                break;
            }
            case OP_GET_SUBSCRIPT: {
                // O índice está no topo da pilha, e o array logo abaixo.
                SapphireValue index_val = pop();
                SapphireValue array_val = pop();

                // 1. Verifica se estamos mesmo tentando acessar um array.
                if (!std::holds_alternative<std::shared_ptr<SapphireArray>>(array_val._value)) {
                    std::cerr << "Runtime Error: Subscript target must be an array." << std::endl;
                    push(SapphireValue());
                    return true;
                }
                auto array_obj = std::get<std::shared_ptr<SapphireArray>>(array_val._value);
                
                // 2. Verifica se o índice é um número.
                if (!std::holds_alternative<double>(index_val._value)) {
                    std::cerr << "Runtime Error: Array index must be a number." << std::endl;
                    push(SapphireValue()); // push nil onto the stack
                    return true;
                }
                double index_double = std::get<double>(index_val._value);
                int index = static_cast<int>(index_double);
                
                // 3. Verifica se o índice está dentro dos limites do array (bounds checking).
                if (index < 0 || index >= array_obj->elements.size()) {
                    std::cerr << "Runtime Error: Array index out of bounds." << std::endl;
                    push(SapphireValue()); // push nil onto the stack
                    return true;
                }
                
                // 4. Se tudo estiver certo, pega o elemento e o coloca na pilha.
                push(array_obj->elements[index]);
                break;
            }
            case OP_SET_SUBSCRIPT: {
                // A ordem na pilha (do topo para baixo) é: valor, índice, array.
                SapphireValue value = pop();
                SapphireValue index_val = pop();
                SapphireValue array_val = pop();

                // Verifica se o alvo é mesmo um array.
                if (!std::holds_alternative<std::shared_ptr<SapphireArray>>(array_val._value)) {
                    std::cerr << "Runtime Error: Subscript target must be an array." << std::endl;
                    return false;
                }
                auto array_obj = std::get<std::shared_ptr<SapphireArray>>(array_val._value);
                
                // Verifica se o índice é um número.
                if (!std::holds_alternative<double>(index_val._value)) {
                    std::cerr << "Runtime Error: Array index must be a number." << std::endl;
                    return false;
                }
                int index = static_cast<int>(std::get<double>(index_val._value));
                
                // Verifica se o índice está dentro dos limites (bounds checking).
                if (index < 0 || index >= array_obj->elements.size()) {
                    std::cerr << "Runtime Error: Array index out of bounds for assignment." << std::endl;
                    return false;
                }
                
                // Se tudo estiver certo, atualiza o valor no array.
                array_obj->elements[index] = value;

                // Colocamos o valor atribuído de volta na pilha, pois a atribuição
                // em si é uma expressão que tem um valor.
                push(value); 
                break;
            }
             default:
                std::cerr << "Erro de Runtime: Opcode desconhecido " << (int)instruction << std::endl;
                return false;
        }
    }
}


bool VM::interpret(const std::string& source) {
    ObjFunction* function = compile(source);
    if (function == nullptr) return false;

    push(function);
    call(function, 0);

    #ifdef DEBUG_PRINT_CODE
        disassemble_chunk(function->chunk, "Script Principal");
    #endif

    return run();
}
