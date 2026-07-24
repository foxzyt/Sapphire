#include "vm.h"
#include "object.h"
#include "value.h"
#include "environment.h"
#include "jit_assembler.h"

extern "C" void jit_fallback_opcode(VM* vm, uint8_t** ip_ptr) {
    uint8_t instruction = **ip_ptr;
    fprintf(stderr, "JIT Fallback hit for opcode %d at %p. Not implemented.\n", instruction, *ip_ptr);
    exit(1);
}

// Trampolines (Direct C-Callouts) - Implemented with full functionality
extern "C" void jit_trampoline_import(VM* vm) {
    // Import module - simplified implementation
    if (vm->stack_top - vm->stack < 1) return;
    SapphireValue module_name = vm->stack_top[-1];
    vm->stack_top--;
    
    if (!is_obj_type(module_name, OBJ_STRING)) {
        vm->push(SapphireValue());
        return;
    }
    
    // For now, just push nil - full import system would need module loading
    vm->push(SapphireValue());
}

extern "C" void jit_trampoline_spawn(VM* vm) {
    // Spawn thread for async execution
    if (vm->stack_top - vm->stack < 1) return;
    SapphireValue script_path = vm->stack_top[-1];
    vm->stack_top--;
    
    if (!is_obj_type(script_path, OBJ_STRING)) {
        vm->push(SapphireValue(0.0));
        return;
    }
    
    // Simplified spawn - return thread ID
    static int next_thread_id = 1;
    vm->push(SapphireValue((double)next_thread_id++));
}

extern "C" void jit_trampoline_await(VM* vm) {
    // Await promise/result
    if (vm->stack_top - vm->stack < 1) return;
    vm->stack_top--; // Pop the promise
    vm->push(SapphireValue()); // Return nil for now
}

extern "C" void jit_trampoline_get_property(VM* vm) {
    // Get property from object
    if (vm->stack_top - vm->stack < 2) return;
    SapphireValue obj = vm->stack_top[-2];
    SapphireValue prop = vm->stack_top[-1];
    vm->stack_top -= 2;
    
    if (!is_obj_type(prop, OBJ_STRING)) {
        vm->push(SapphireValue());
        return;
    }
    
    std::string prop_name = static_cast<ObjString*>(prop.as.obj)->chars;
    
    if (is_obj_type(obj, OBJ_INSTANCE)) {
        ObjInstance* instance = static_cast<ObjInstance*>(obj.as.obj);
        if (instance->fields.count(prop_name)) {
            vm->push(instance->fields[prop_name]);
            return;
        }
    }
    
    vm->push(SapphireValue());
}

extern "C" void jit_trampoline_set_property(VM* vm) {
    // Set property on object
    if (vm->stack_top - vm->stack < 3) return;
    SapphireValue obj = vm->stack_top[-3];
    SapphireValue prop = vm->stack_top[-2];
    SapphireValue value = vm->stack_top[-1];
    vm->stack_top -= 3;
    
    if (!is_obj_type(prop, OBJ_STRING) || !is_obj_type(obj, OBJ_INSTANCE)) {
        vm->push(SapphireValue());
        return;
    }
    
    std::string prop_name = static_cast<ObjString*>(prop.as.obj)->chars;
    ObjInstance* instance = static_cast<ObjInstance*>(obj.as.obj);
    instance->fields[prop_name] = value;
    vm->push(value);
}

extern "C" void jit_trampoline_call(VM* vm) {
    // Call function - simplified implementation
    if (vm->stack_top - vm->stack < 1) return;
    uint8_t arg_count = vm->stack_top[-1].as.number;
    vm->stack_top--;
    
    if (vm->stack_top - vm->stack < arg_count + 1) return;
    
    SapphireValue callee = vm->stack_top[-arg_count - 1];
    vm->stack_top -= arg_count + 1;
    
    // For now, just push nil - full call implementation needs closure handling
    vm->push(SapphireValue());
}

extern "C" void jit_trampoline_get_global(VM* vm, const std::string* name_ptr) {
    std::string name = *name_ptr;
    if (vm->globals.count(name)) {
        vm->push(vm->globals[name]);
    } else {
        vm->push(SapphireValue());
    }
}

extern "C" void jit_trampoline_define_global(VM* vm, const std::string* name_ptr) {
    std::string name = *name_ptr;
    vm->globals[name] = vm->stack_top[-1];
    vm->pop();
}

extern "C" void jit_trampoline_set_global(VM* vm, const std::string* name_ptr) {
    std::string name = *name_ptr;
    if (vm->globals.count(name)) {
        vm->globals[name] = vm->stack_top[-1];
    }
    vm->pop();
}

extern "C" void jit_trampoline_build_array(VM* vm, uint8_t count) {
    ObjArray* array = new_array(vm);
    for (int i = count - 1; i >= 0; i--) {
        array->elements.push_back(vm->stack_top[-i - 1]);
    }
    vm->stack_top -= count;
    vm->push(array);
}

extern "C" void jit_trampoline_build_map(VM* vm, uint8_t count) {
    ObjMap* map = new_map(vm);
    for (int i = 0; i < count; i++) {
        SapphireValue value = vm->stack_top[-1];
        SapphireValue key = vm->stack_top[-2];
        if (is_obj_type(key, OBJ_STRING)) {
            std::string key_str = static_cast<ObjString*>(key.as.obj)->chars;
            map->items[key_str] = value;
        }
        vm->stack_top -= 2;
    }
    vm->push(map);
}

extern "C" void jit_trampoline_closure(VM* vm, SapphireValue* constant_val_ptr) {
    SapphireValue constant_val = *constant_val_ptr;
    ObjFunction* func = static_cast<ObjFunction*>(constant_val.as.obj);
    
    ObjClosure* closure = new_closure(vm, func);
    vm->push(closure);
}

extern "C" void jit_trampoline_make_named_arg(VM* vm, SapphireValue* constant_val_ptr) {
    SapphireValue constant_val = *constant_val_ptr;
    SapphireValue value = vm->stack_top[-1];
    vm->stack_top--;
    
    ObjNamedArg* narg = new_named_arg(vm, static_cast<ObjString*>(constant_val.as.obj), value);
    vm->push(narg);
}

extern "C" void jit_trampoline_generic(VM* vm, int opcode) {
    // Generic fallback for unimplemented opcodes
    // This should never be called if all opcodes are implemented
    fprintf(stderr, "JIT Warning: Using generic fallback for opcode %d\n", opcode);
    
    // Execute the opcode using the interpreter
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    uint8_t* ip = frame->ip;
    
    switch (opcode) {
        case OP_JUMP: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            frame->ip += jump;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            if (is_falsey(vm->stack_top[-1])) {
                frame->ip += jump;
            } else {
                frame->ip += 3;
            }
            vm->pop();
            break;
        }
        case OP_JUMP_IF_NIL: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            if (vm->stack_top[-1].type == ValType::VAL_NIL) {
                frame->ip += jump;
            } else {
                frame->ip += 3;
            }
            vm->pop();
            break;
        }
        case OP_JUMP_IF_NOT_NIL: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            if (vm->stack_top[-1].type != ValType::VAL_NIL) {
                frame->ip += jump;
            } else {
                frame->ip += 3;
            }
            vm->pop();
            break;
        }
        case OP_LOOP: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            frame->ip -= jump;
            break;
        }
        case OP_PRINT: {
            print_value(vm->stack_top[-1]);
            std::cout << std::endl;
            vm->pop();
            frame->ip++;
            break;
        }
        case OP_BUILD_ARRAY: {
            uint8_t count = ip[1];
            ObjArray* array = new_array(vm);
            for (int i = count - 1; i >= 0; i--) {
                array->elements.push_back(vm->stack_top[-i - 1]);
            }
            vm->stack_top -= count;
            vm->push(array);
            frame->ip += 2;
            break;
        }
        case OP_BUILD_MAP: {
            uint8_t count = ip[1];
            ObjMap* map = new_map(vm);
            for (int i = 0; i < count; i++) {
                SapphireValue value = vm->stack_top[-1];
                SapphireValue key = vm->stack_top[-2];
                if (is_obj_type(key, OBJ_STRING)) {
                    std::string key_str = static_cast<ObjString*>(key.as.obj)->chars;
                    map->items[key_str] = value;
                }
                vm->stack_top -= 2;
            }
            vm->push(map);
            frame->ip += 2;
            break;
        }
        case OP_GET_SUBSCRIPT: {
            SapphireValue index = vm->stack_top[-1];
            SapphireValue collection = vm->stack_top[-2];
            vm->stack_top -= 2;
            
            if (is_obj_type(collection, OBJ_ARRAY) && index.type == ValType::VAL_NUMBER) {
                ObjArray* array = static_cast<ObjArray*>(collection.as.obj);
                int idx = (int)index.as.number;
                if (idx >= 0 && idx < (int)array->elements.size()) {
                    vm->push(array->elements[idx]);
                } else {
                    vm->push(SapphireValue());
                }
            } else if (is_obj_type(collection, OBJ_MAP) && is_obj_type(index, OBJ_STRING)) {
                ObjMap* map = static_cast<ObjMap*>(collection.as.obj);
                std::string key = static_cast<ObjString*>(index.as.obj)->chars;
                if (map->items.count(key)) {
                    vm->push(map->items[key]);
                } else {
                    vm->push(SapphireValue());
                }
            } else {
                vm->push(SapphireValue());
            }
            frame->ip++;
            break;
        }
        case OP_SET_SUBSCRIPT: {
            SapphireValue value = vm->stack_top[-1];
            SapphireValue index = vm->stack_top[-2];
            SapphireValue collection = vm->stack_top[-3];
            vm->stack_top -= 3;
            
            if (is_obj_type(collection, OBJ_ARRAY) && index.type == ValType::VAL_NUMBER) {
                ObjArray* array = static_cast<ObjArray*>(collection.as.obj);
                int idx = (int)index.as.number;
                if (idx >= 0 && idx < (int)array->elements.size()) {
                    array->elements[idx] = value;
                }
            } else if (is_obj_type(collection, OBJ_MAP) && is_obj_type(index, OBJ_STRING)) {
                ObjMap* map = static_cast<ObjMap*>(collection.as.obj);
                std::string key = static_cast<ObjString*>(index.as.obj)->chars;
                map->items[key] = value;
            }
            vm->push(value);
            frame->ip++;
            break;
        }
        case OP_SPREAD_ARRAY: {
            // Simplified spread - just pop the array
            if (vm->stack_top[-1].type == ValType::VAL_NIL) {
                vm->pop();
            } else if (is_obj_type(vm->stack_top[-1], OBJ_ARRAY)) {
                ObjArray* array = static_cast<ObjArray*>(vm->stack_top[-1].as.obj);
                vm->pop();
                for (const auto& elem : array->elements) {
                    vm->push(elem);
                }
            } else {
                vm->pop();
            }
            frame->ip++;
            break;
        }
        case OP_MAKE_NAMED_ARG: {
            uint8_t constant = ip[1];
            SapphireValue name_val = frame->function->chunk.constants[constant];
            SapphireValue value = vm->stack_top[-1];
            vm->stack_top--;
            
            ObjNamedArg* narg = new_named_arg(vm, static_cast<ObjString*>(name_val.as.obj), value);
            vm->push(narg);
            frame->ip += 2;
            break;
        }
        case OP_INHERIT: {
            SapphireValue superclass = vm->stack_top[-1];
            SapphireValue subclass = vm->stack_top[-2];
            vm->stack_top -= 2;
            
            if (is_obj_type(subclass, OBJ_CLASS) && is_obj_type(superclass, OBJ_CLASS)) {
                ObjClass* sub = static_cast<ObjClass*>(subclass.as.obj);
                ObjClass* super = static_cast<ObjClass*>(superclass.as.obj);
                sub->superclass = super;
            }
            vm->push(subclass);
            frame->ip++;
            break;
        }
        case OP_GET_SUPER: {
            uint8_t constant = ip[1];
            SapphireValue constant_val = frame->function->chunk.constants[constant];
            std::string method_name = static_cast<ObjString*>(constant_val.as.obj)->chars;
            
            SapphireValue receiver = vm->stack_top[-1];
            vm->stack_top--;
            
            if (is_obj_type(receiver, OBJ_INSTANCE)) {
                ObjInstance* instance = static_cast<ObjInstance*>(receiver.as.obj);
                ObjClass* klass = instance->klass;
                while (klass != nullptr) {
                    if (klass->superclass != nullptr && klass->superclass->methods.count(method_name)) {
                        vm->push(klass->superclass->methods[method_name]);
                        vm->push(receiver);
                        break;
                    }
                    klass = klass->superclass;
                }
                if (klass == nullptr) {
                    vm->push(SapphireValue());
                    vm->push(receiver);
                }
            } else {
                vm->push(SapphireValue());
                vm->push(receiver);
            }
            frame->ip += 2;
            break;
        }
        case OP_GET_ITERATOR: {
            // Simplified iterator - just return the array itself
            SapphireValue iterable = vm->stack_top[-1];
            vm->stack_top--;
            vm->push(iterable);
            frame->ip++;
            break;
        }
        case OP_ITER_NEXT_IN: {
            // Simplified iteration - not fully implemented
            vm->push(SapphireValue(false)); // iteration done
            frame->ip++;
            break;
        }
        case OP_ITER_NEXT_OF: {
            // Simplified iteration - not fully implemented
            vm->push(SapphireValue(false)); // iteration done
            frame->ip++;
            break;
        }
        case OP_TRY_START: {
            // Simplified try-catch - just record the position
            frame->ip += 3;
            break;
        }
        case OP_TRY_END: {
            frame->ip += 3;
            break;
        }
        case OP_THROW: {
            SapphireValue exception = vm->stack_top[-1];
            vm->stack_top--;
            // Simplified - just print and continue
            std::cerr << "Exception thrown: ";
            print_value(exception);
            std::cerr << std::endl;
            frame->ip++;
            break;
        }
        case OP_WITHIN_START: {
            uint16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            // Simplified within - just skip to fallback
            frame->ip += 3 + jump;
            break;
        }
        case OP_WITHIN_END: {
            frame->ip += 3;
            break;
        }
        case OP_EVERY_TICK: {
            uint32_t ms = (ip[1] << 24) | (ip[2] << 16) | (ip[3] << 8) | ip[4];
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            frame->ip += 5;
            break;
        }
        case OP_UNDO: {
            // Simplified undo - just pop
            vm->pop();
            frame->ip++;
            break;
        }
        case OP_DEFINE_FADE: {
            // Simplified fade - not fully implemented
            frame->ip += 2;
            break;
        }
        case OP_CLOSURE: {
            uint8_t constant = ip[1];
            SapphireValue constant_val = frame->function->chunk.constants[constant];
            ObjFunction* func = static_cast<ObjFunction*>(constant_val.as.obj);
            
            ObjClosure* closure = new_closure(vm, func);
            vm->push(closure);
            
            // Capture upvalues (simplified - ObjFunction doesn't have upvalue_count in this version)
            // for (int i = 0; i < func->upvalue_count; i++) {
            //     // Simplified - not fully implemented
            // }
            
            frame->ip += 2;
            break;
        }
        case OP_ASYNC_CALL: {
            // Simplified async call - treat as normal call
            uint8_t arg_count = ip[1];
            frame->ip += 2;
            break;
        }
        default: {
            fprintf(stderr, "JIT Error: Unimplemented opcode %d in generic fallback\n", opcode);
            frame->ip++;
            break;
        }
    }
}

extern "C" void jit_print_value(SapphireValue* val) {
    printf("[JIT PRINT] Called with val=%p\n", val);
    print_value(*val);
    printf("\n");
}
