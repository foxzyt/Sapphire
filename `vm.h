#ifndef VM_H
#define VM_H

#include <vector>
#include <string>

enum class GCState {
    GC_IDLE,
    GC_MARK_ROOTS,
    GC_TRACE,
    GC_SWEEP
};

class VM {
public:
    enum class GCState {
        GC_IDLE,
        GC_MARK_ROOTS,
        GC_TRACE,
        GC_SWEEP
    };
    GCState gc_state = GCState::GC_IDLE;
    Obj* sweep_previous = nullptr;
    Obj* sweep_current = nullptr;

    friend void debug_print_stack(VM *vm);
};

#endif // VM_H
