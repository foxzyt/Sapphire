// gc.cpp — Sapphire Garbage Collector (v1.0.9 — Turbo + Safe)
// Corrections over v1.0.9:
//  - mark_value: fixed broken VAL_OBJ + OBJ_ARRAY detection (was unreachable code)
//  - blacken_object: added OBJ_MAP, OBJ_ARRAY, OBJ_LRU, OBJ_PROMISE, OBJ_NAMED_ARG, OBJ_FADE
//  - adaptive threshold: next_gc uses a smoother growth factor
//  - sweep: accurate size accounting for all object types
#include "vm.h"
#include "object.h"
#include "value.h"
#include "environment.h"
#include <iostream>

void VM::mark_object(Obj* object) {
    if (object == nullptr || object->is_marked) return;

    object->is_marked = true;
    gray_stack.push_back(object);
}

// FIX v1.0.9: previous version had unreachable OBJ_ARRAY check inside the
// VAL_OBJ branch that returned early. Now we handle all object types properly.
void VM::mark_value(SapphireValue value) {
    if (value.type != ValType::VAL_OBJ || value.as.obj == nullptr) return;
    mark_object(value.as.obj);
}

void VM::blacken_object(Obj* object) {
    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = static_cast<ObjClosure*>(object);
            mark_object(static_cast<Obj*>(closure->function));
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = static_cast<ObjFunction*>(object);
            mark_object(static_cast<Obj*>(function->name));
            if (function->owner_class) mark_object(static_cast<Obj*>(function->owner_class));
            for (SapphireValue& constant : function->chunk.constants) {
                mark_value(constant);
            }
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = static_cast<ObjInstance*>(object);
            mark_object(static_cast<Obj*>(instance->klass));
            for (auto const& [key, val] : instance->fields) {
                mark_value(val);
            }
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = static_cast<ObjClass*>(object);
            mark_object(static_cast<Obj*>(klass->name));
            if (klass->superclass) mark_object(static_cast<Obj*>(klass->superclass));
            for (auto const& [key, val] : klass->methods) {
                mark_value(val);
            }
            break;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = static_cast<ObjBoundMethod*>(object);
            mark_value(bound->receiver);
            mark_value(bound->method);
            if (bound->defined_in_class) mark_object(static_cast<Obj*>(bound->defined_in_class));
            break;
        }
        // FIX v1.0.9: OBJ_ARRAY was never blackened — child values could be freed
        case OBJ_ARRAY: {
            ObjArray* array = static_cast<ObjArray*>(object);
            for (SapphireValue& val : array->elements) {
                mark_value(val);
            }
            break;
        }
        // FIX v1.0.9: OBJ_MAP was never blackened — map values could be freed
        case OBJ_MAP: {
            ObjMap* map = static_cast<ObjMap*>(object);
            for (auto const& [key, val] : map->items) {
                mark_value(val);
            }
            break;
        }
        // FIX v1.0.9: OBJ_LRU was never blackened
        case OBJ_LRU: {
            ObjLRU* lru = static_cast<ObjLRU*>(object);
            for (auto const& [key, val] : lru->items) {
                mark_value(val);
            }
            break;
        }
        // FIX v1.0.9: OBJ_PROMISE was never blackened — saved stack/frames leaked
        case OBJ_PROMISE: {
            ObjPromise* promise = static_cast<ObjPromise*>(object);
            mark_value(promise->value);
            if (promise->function) mark_object(static_cast<Obj*>(promise->function));
            for (SapphireValue& arg : promise->args) {
                mark_value(arg);
            }
            for (SapphireValue& sv : promise->saved_stack) {
                mark_value(sv);
            }
            for (ObjPromise* awaiter : promise->awaiters) {
                mark_object(static_cast<Obj*>(awaiter));
            }
            break;
        }
        // FIX v1.0.9: OBJ_NAMED_ARG was never blackened
        case OBJ_NAMED_ARG: {
            ObjNamedArg* named = static_cast<ObjNamedArg*>(object);
            mark_object(static_cast<Obj*>(named->name));
            mark_value(named->value);
            break;
        }
        // FIX v1.0.9: OBJ_FADE was never blackened
        case OBJ_FADE: {
            ObjFade* fade = static_cast<ObjFade*>(object);
            mark_value(fade->value);
            break;
        }
        case OBJ_NATIVE: {
            ObjNative* native = static_cast<ObjNative*>(object);
            if (native->name) mark_object(static_cast<Obj*>(native->name));
            break;
        }
        case OBJ_STRING:
            // Strings have no child references
            break;
    }
}

void VM::mark_roots() {
    // Mark the value stack
    for (SapphireValue* slot = stack; slot < stack_top; slot++) {
        mark_value(*slot);
    }
    // Mark all call frame functions
    for (int i = 0; i < frame_count; i++) {
        mark_object(static_cast<Obj*>(frames[i].function));
    }
    // Mark globals
    for (auto const& [key, val] : globals) {
        mark_value(val);
    }
    // Mark event loop promises
    for (ObjPromise* promise : event_loop_queue) {
        mark_object(static_cast<Obj*>(promise));
    }
    if (current_promise) {
        mark_object(static_cast<Obj*>(current_promise));
    }
    // Mark catch block stack values
    for (int i = 0; i < catch_count; i++) {
        // catch_blocks hold stack pointers but no GC roots — no action needed
    }
}

void VM::write_barrier(Obj* object, SapphireValue value) {
    if (gc_state == GCState::GC_TRACE) {
        if (object != nullptr && object->is_marked) {
            mark_value(value);
        }
    }
}

void VM::step_gc() {
    if (gc_state == GCState::GC_IDLE) {
        if (bytes_allocated > next_gc_threshold) {
            gc_state = GCState::GC_MARK_ROOTS;
        } else {
            return;
        }
    }

    if (gc_state == GCState::GC_MARK_ROOTS) {
        mark_roots();
        gc_state = GCState::GC_TRACE;
        return;
    }

    if (gc_state == GCState::GC_TRACE) {
        // Process gray objects in batches to avoid long pauses
        constexpr int trace_limit = 500;
        int processed = 0;
        while (!gray_stack.empty() && processed < trace_limit) {
            Obj* object = gray_stack.back();
            gray_stack.pop_back();
            blacken_object(object);
            processed++;
        }

        if (gray_stack.empty()) {
            // Final remark: rescan roots to catch mutations during tracing
            mark_roots();
            if (gray_stack.empty()) {
                gc_state = GCState::GC_SWEEP;
                sweep_previous = nullptr;
                sweep_current = objects;
            }
        }
        return;
    }

    if (gc_state == GCState::GC_SWEEP) {
        constexpr int sweep_limit = 500;
        int swept = 0;

        while (sweep_current != nullptr && swept < sweep_limit) {
            Obj* object = sweep_current;

            if (object->is_marked) {
                // Survivor: clear mark for next cycle
                object->is_marked = false;
                sweep_previous = object;
                sweep_current = object->next;
            } else {
                // Unreachable: remove from linked list and free
                Obj* unreached = object;
                sweep_current = object->next;

                if (sweep_previous != nullptr) {
                    sweep_previous->next = sweep_current;
                } else {
                    objects = sweep_current;
                }

                // Accurate size accounting for all types
                size_t size = 0;
                switch (unreached->type) {
                    case OBJ_STRING:        size = sizeof(ObjString); break;
                    case OBJ_FUNCTION:      size = sizeof(ObjFunction); break;
                    case OBJ_NATIVE:        size = sizeof(ObjNative); break;
                    case OBJ_CLOSURE:       size = sizeof(ObjClosure); break;
                    case OBJ_CLASS:         size = sizeof(ObjClass); break;
                    case OBJ_INSTANCE:      size = sizeof(ObjInstance); break;
                    case OBJ_BOUND_METHOD:  size = sizeof(ObjBoundMethod); break;
                    case OBJ_NAMED_ARG:     size = sizeof(ObjNamedArg); break;
                    case OBJ_MAP:           size = sizeof(ObjMap); break;
                    case OBJ_ARRAY:         size = sizeof(ObjArray); break;
                    case OBJ_LRU:           size = sizeof(ObjLRU); break;
                    case OBJ_PROMISE:       size = sizeof(ObjPromise); break;
                    case OBJ_FADE:          size = sizeof(ObjFade); break;
                }

                if (bytes_allocated >= size) bytes_allocated -= size;
                else bytes_allocated = 0;

                free_object(unreached);
            }
            swept++;
        }

        if (sweep_current == nullptr) {
            gc_state = GCState::GC_IDLE;

            // Adaptive threshold: grow smoothly — 1.5× live data, min 1MB, max 500MB
            constexpr size_t min_threshold = 1024 * 1024;          // 1 MB
            constexpr size_t max_threshold = 500 * 1024 * 1024;    // 500 MB
            next_gc_threshold = bytes_allocated + (bytes_allocated / 2);
            if (next_gc_threshold < min_threshold) next_gc_threshold = min_threshold;
            if (next_gc_threshold > max_threshold) next_gc_threshold = max_threshold;
        }
    }
}
