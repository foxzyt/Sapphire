#ifndef SAPPHIRE_VM_H
#define SAPPHIRE_VM_H

#include "chunk.h"
#include "value.h"
#include "object.h" // Incluído para ObjFunction
#include <unordered_map>
#include <string>

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * 256)

// Representa um único quadro de chamada na pilha de chamadas da VM.
struct CallFrame {
    ObjFunction* function;
    uint8_t* ip;        // Instruction Pointer
    SapphireValue* slots; // Ponteiro para o slot da VM onde o quadro começa
};

class VM {
public:
    VM();
    bool interpret(const std::string& source);

private:
    CallFrame frames[FRAMES_MAX];
    int frame_count;
    friend void debug_print_stack(VM* vm);

    SapphireValue stack[STACK_MAX];
    SapphireValue* stack_top;

    std::unordered_map<std::string, SapphireValue> globals;

    bool run();
    void push(const SapphireValue& value);
    SapphireValue pop();
    SapphireValue& peek(int distance);

    bool call(ObjFunction* function, int arg_count);
    bool call_value(SapphireValue callee, int arg_count);

    void define_native(const std::string& name, NativeFn function);
};

#endif //SAPPHIRE_VM_H