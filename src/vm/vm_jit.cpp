#include "vm.h"
#include "object.h"
#include "value.h"
#include "environment.h"
#include "jit_assembler.h"
#include <cstdio>

// Forward declaration for interpreter fallback
bool vm_interpreter_fallback(VM* vm, int target_frame_count);

bool VM::run(int target_frame_count) {
    CallFrame* frame = &frames[frame_count - 1];
    Chunk* chunk = &frame->function->chunk;
    
    if (rubellite_debug) {
        printf("[JIT DEBUG] Starting JIT compilation for function with %zu opcodes\n", chunk->code.size());
    }
    
    std::vector<uint8_t> optimized_code = chunk->code;
    std::vector<SapphireValue> optimized_constants = chunk->constants;
    
    // FIX v1.1.0: constant folding with proper bounds guards to prevent OOB access
    for (size_t i = 0; i < optimized_code.size(); ) {
        uint8_t opcode = optimized_code[i];

        // Constant folding: CONST A, CONST B, OP → CONST (A op B)
        // Guard: need at least 4 bytes before current position (OP_CONSTANT, idx, OP_CONSTANT, idx)
        if ((opcode == OP_ADD || opcode == OP_SUBTRACT || opcode == OP_MULTIPLY || opcode == OP_DIVIDE) &&
            i >= 4 &&
            optimized_code[i - 2] == OP_CONSTANT &&
            optimized_code[i - 4] == OP_CONSTANT) {

            uint8_t const1_idx = optimized_code[i - 1]; // top of stack
            uint8_t const2_idx = optimized_code[i - 3]; // second on stack

            // Bounds-check constant indices before accessing
            if (const1_idx < optimized_constants.size() && const2_idx < optimized_constants.size()) {
                SapphireValue val1 = optimized_constants[const1_idx];
                SapphireValue val2 = optimized_constants[const2_idx];

                if (val1.type == ValType::VAL_NUMBER && val2.type == ValType::VAL_NUMBER) {
                    double result = 0.0;
                    bool valid = true;
                    if (opcode == OP_ADD)      result = val2.as.number + val1.as.number;
                    else if (opcode == OP_SUBTRACT) result = val2.as.number - val1.as.number;
                    else if (opcode == OP_MULTIPLY) result = val2.as.number * val1.as.number;
                    else if (opcode == OP_DIVIDE) {
                        if (val1.as.number != 0.0) result = val2.as.number / val1.as.number;
                        else valid = false; // skip folding for div-by-zero
                    }

                    if (valid && optimized_constants.size() < 255) {
                        optimized_constants.push_back(SapphireValue(result));
                        uint8_t new_const_idx = static_cast<uint8_t>(optimized_constants.size() - 1);
                        // Replace 4-byte sequence (CONST idx CONST idx) + opcode (1) = 5 bytes → 2 bytes
                        optimized_code.erase(optimized_code.begin() + (i - 4), optimized_code.begin() + i + 1);
                        optimized_code.insert(optimized_code.begin() + (i - 4), new_const_idx);
                        optimized_code.insert(optimized_code.begin() + (i - 4), OP_CONSTANT);
                        // i now points to the new OP_CONSTANT; restart from same position
                        i = (i >= 4) ? (i - 4) : 0;
                        continue;
                    }
                }
            }
        } else if (opcode == OP_JUMP_IF_FALSE && i >= 2 && optimized_code[i - 2] == OP_CONSTANT) {
            uint8_t const_idx = optimized_code[i - 1];
            if (const_idx < optimized_constants.size()) {
                SapphireValue val = optimized_constants[const_idx];
                if (is_falsey(val)) {
                    optimized_code[i] = OP_JUMP;
                } else {
                    // Always-true branch: remove the conditional jump (3 bytes)
                    if (i + 3 <= optimized_code.size()) {
                        optimized_code.erase(optimized_code.begin() + i, optimized_code.begin() + i + 3);
                    }
                    continue;
                }
            }
        }
        i++;
    }
    
    Chunk optimized_chunk;
    optimized_chunk.code = optimized_code;
    optimized_chunk.constants = optimized_constants;
    chunk = &optimized_chunk;
    
    if (rubellite_debug) {
        printf("[JIT DEBUG] Optimization complete. %zu opcodes -> %zu opcodes\n", 
               frame->function->chunk.code.size(), optimized_code.size());
    }
    
    JitAssembler jit;
    
    jit.emit_push_reg(3); 
    jit.emit_push_reg(5); 
    jit.emit_push_reg(6); 
    jit.emit_push_reg(7); 
    jit.emit_push_reg(12); 
    jit.emit_push_reg(13); 
    jit.emit_push_reg(14); 
    jit.emit_push_reg(15); 

    jit.emit_mov_reg_imm64(13, (uint64_t)this);
    
    size_t stack_top_offset = offsetof(VM, stack_top);
    jit.emit_mov_reg_reg(14, 13);
    jit.emit_add_reg_imm32(14, stack_top_offset);
    
    jit.emit_mov_reg_mem(12, 14, 0);

    size_t globals_offset = offsetof(VM, globals);
    jit.emit_mov_reg_reg(15, 13);
    jit.emit_add_reg_imm32(15, globals_offset);

    std::vector<JitAssembler::Label> labels;
    labels.reserve(chunk->code.size());
    for (size_t i = 0; i < chunk->code.size(); i++) {
        labels.push_back(jit.new_label());
    }

    static void* dispatch_table[] = {
        &&TARGET_OP_CONSTANT, &&TARGET_OP_NIL, &&TARGET_OP_TRUE, &&TARGET_OP_FALSE,
        &&TARGET_OP_POP, &&TARGET_OP_DUP,
        &&TARGET_OP_GET_LOCAL, &&TARGET_OP_SET_LOCAL, &&TARGET_OP_GET_GLOBAL, &&TARGET_OP_DEFINE_GLOBAL, &&TARGET_OP_SET_GLOBAL, &&TARGET_OP_GET_PROPERTY, &&TARGET_OP_SET_PROPERTY,
        &&TARGET_OP_BUILD_ARRAY, &&TARGET_OP_BUILD_MAP, &&TARGET_OP_GET_SUBSCRIPT, &&TARGET_OP_SET_SUBSCRIPT, &&TARGET_OP_SPREAD_ARRAY,
        &&TARGET_OP_EQUAL, &&TARGET_OP_GREATER, &&TARGET_OP_LESS, &&TARGET_OP_NOT,
        &&TARGET_OP_ADD, &&TARGET_OP_SUBTRACT, &&TARGET_OP_MULTIPLY, &&TARGET_OP_DIVIDE, &&TARGET_OP_MODULO, &&TARGET_OP_NEGATE,
        &&TARGET_OP_BITWISE_AND, &&TARGET_OP_BITWISE_OR, &&TARGET_OP_BITWISE_XOR, &&TARGET_OP_BITWISE_NOT, &&TARGET_OP_LEFT_SHIFT, &&TARGET_OP_RIGHT_SHIFT,
        &&TARGET_OP_PRINT, &&TARGET_OP_JUMP, &&TARGET_OP_JUMP_IF_FALSE, &&TARGET_OP_JUMP_IF_NIL, &&TARGET_OP_JUMP_IF_NOT_NIL, &&TARGET_OP_LOOP, 
        &&TARGET_OP_CALL, &&TARGET_OP_CLOSURE, &&TARGET_OP_RETURN, &&TARGET_OP_IMPORT, 
        &&TARGET_OP_MAKE_NAMED_ARG, &&TARGET_OP_INHERIT, &&TARGET_OP_GET_SUPER, 
        &&TARGET_OP_SPAWN, &&TARGET_OP_AWAIT, &&TARGET_OP_ASYNC_CALL,
        &&TARGET_OP_GET_ITERATOR, &&TARGET_OP_ITER_NEXT_IN, &&TARGET_OP_ITER_NEXT_OF,
        &&TARGET_OP_TRY_START, &&TARGET_OP_TRY_END, &&TARGET_OP_THROW,
        &&TARGET_OP_WITHIN_START, &&TARGET_OP_WITHIN_END, &&TARGET_OP_EVERY_TICK, 
        &&TARGET_OP_UNDO, &&TARGET_OP_DEFINE_FADE,
        &&TARGET_OP_SUPER, &&TARGET_OP_THIS, &&TARGET_OP_CLASS
    };

    auto emit_trampoline = [&](void* func) {
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(0, (uint64_t)func);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
    };

    auto emit_trampoline_with_opcode = [&](void* func, int op) {
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, op);
        jit.emit_mov_reg_imm64(0, (uint64_t)func);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
    };

    int offset = 0;
    uint8_t opcode = 0;
    
    if (chunk->code.size() == 0) goto JIT_END;

NEXT_OPCODE:
    if (offset >= chunk->code.size()) goto JIT_END;
    
    jit.bind(labels[offset]);
    
    opcode = chunk->code[offset];
    if (opcode >= sizeof(dispatch_table)/sizeof(dispatch_table[0])) {
        goto JIT_END;
    }
    goto *dispatch_table[opcode];

    TARGET_OP_CONSTANT: {
        uint8_t constant_idx = chunk->code[offset + 1];
        SapphireValue* val_ptr = &chunk->constants[constant_idx];
        jit.emit_mov_reg_imm64(0, (uint64_t)val_ptr);
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_NIL: {
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_TRUE: {
        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_FALSE: {
        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_POP: {
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_DUP: {
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_LOCAL: {
        uint8_t slot = chunk->code[offset + 1];
        SapphireValue* slot_ptr = &frame->slots[slot];
        jit.emit_mov_reg_imm64(0, (uint64_t)slot_ptr);
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SET_LOCAL: {
        uint8_t slot = chunk->code[offset + 1];
        SapphireValue* slot_ptr = &frame->slots[slot];
        jit.emit_mov_reg_imm64(0, (uint64_t)slot_ptr);
        jit.emit_mov_reg_mem(1, 12, -(int32_t)sizeof(SapphireValue));
        jit.emit_mov_mem_reg(0, 0, 1);
        jit.emit_mov_reg_mem(1, 12, -(int32_t)sizeof(SapphireValue) + 8);
        jit.emit_mov_mem_reg(0, 8, 1);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_GLOBAL: {
        emit_trampoline((void*)&jit_trampoline_get_global);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_DEFINE_GLOBAL: {
        emit_trampoline((void*)&jit_trampoline_define_global);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SET_GLOBAL: {
        emit_trampoline((void*)&jit_trampoline_set_global);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_PROPERTY: {
        JitAssembler::Label is_instance_lbl, not_instance_lbl, end_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(not_instance_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_INSTANCE);
        jit.emit_jz(is_instance_lbl);
        jit.emit_jmp(not_instance_lbl);
        
        jit.bind(is_instance_lbl);
        jit.emit_mov_reg_mem(1, 0, 8 + offsetof(ObjInstance, fields));
        
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_GET_PROPERTY);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_get_property);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(not_instance_lbl);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.bind(end_lbl);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SET_PROPERTY: {
        emit_trampoline((void*)&jit_trampoline_set_property);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_BUILD_ARRAY: {
        uint8_t count = chunk->code[offset + 1];
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(0, (uint64_t)&new_array);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_reg(2, 0);
        
        for (int i = count - 1; i >= 0; i--) {
            jit.emit_mov_reg_reg(0, 12);
            jit.emit_sub_reg_imm32(0, sizeof(SapphireValue) * (i + 1));
            jit.emit_mov_reg_reg(3, 2);
            jit.emit_mov_reg_imm64(4, offsetof(ObjArray, elements));
            jit.emit_add_reg_reg(3, 4);
            jit.emit_mov_mem_reg(14, 0, 12);
            jit.emit_mov_reg_reg(1, 13);
            jit.emit_mov_reg_reg(2, 0);
            jit.emit_mov_reg_imm64(3, OP_BUILD_ARRAY);
            jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_build_array);
            jit.emit_sub_reg_imm32(4, 40);
            jit.emit_call_reg(0);
            jit.emit_add_reg_imm32(4, 40);
            jit.emit_mov_reg_mem(12, 14, 0);
        }
        
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * count);
        jit.emit_mov_reg_reg(0, 2);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 3);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_BUILD_MAP: {
        uint8_t count = chunk->code[offset + 1];
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(0, (uint64_t)&new_map);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_reg(2, 0);
        
        for (int i = 0; i < count; i++) {
            jit.emit_mov_mem_reg(14, 0, 12);
            jit.emit_mov_reg_reg(1, 13);
            jit.emit_mov_reg_reg(2, 0);
            jit.emit_mov_reg_imm64(3, OP_BUILD_MAP);
            jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_build_map);
            jit.emit_sub_reg_imm32(4, 40);
            jit.emit_call_reg(0);
            jit.emit_add_reg_imm32(4, 40);
            jit.emit_mov_reg_mem(12, 14, 0);
        }
        
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * count * 2);
        jit.emit_mov_reg_reg(0, 2);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 3);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_SUBSCRIPT: {
        JitAssembler::Label is_array_lbl, is_map_lbl, not_found_lbl, end_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue) * 2);
        jit.emit_mov_reg_reg(1, 12);
        jit.emit_sub_reg_imm32(1, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(not_found_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_ARRAY);
        jit.emit_jz(is_array_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_MAP);
        jit.emit_jz(is_map_lbl);
        jit.emit_jmp(not_found_lbl);
        
        jit.bind(is_array_lbl);
        jit.emit_cmp_mem8_imm8(1, 0, 2);
        jit.emit_jnz(not_found_lbl);
        jit.emit_mov_reg_mem(2, 1, 8);
        jit.emit_movsd_xmm_mem(0, 1, 8);
        jit.emit_cvttsd2si_reg_xmm(3, 0);
        
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_GET_SUBSCRIPT);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(is_map_lbl);
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_GET_SUBSCRIPT);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(not_found_lbl);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SET_SUBSCRIPT: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_SET_SUBSCRIPT);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SPREAD_ARRAY: {
        JitAssembler::Label not_array_lbl, end_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(not_array_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_ARRAY);
        jit.emit_jnz(not_array_lbl);
        
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_SPREAD_ARRAY);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(not_array_lbl);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_EQUAL:
    TARGET_OP_GREATER:
    TARGET_OP_LESS: {
        JitAssembler::Label fallback_lbl, is_true_lbl, cont_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);
        jit.emit_ucomisd_xmm_xmm(0, 1);

        uint8_t instruction = chunk->code[offset];
        if (instruction == OP_EQUAL) jit.emit_jz(is_true_lbl);
        else if (instruction == OP_GREATER) jit.emit_ja(is_true_lbl);
        else if (instruction == OP_LESS) jit.emit_jb(is_true_lbl);

        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_jmp(cont_lbl);

        jit.bind(is_true_lbl);
        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_mem_reg(12, 8, 0);

        jit.bind(cont_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, instruction);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_NOT: {
        JitAssembler::Label is_false_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 0);
        jit.emit_jz(is_false_lbl);

        jit.emit_cmp_mem8_imm8(0, 0, 1);
        jit.emit_jnz(end_lbl);

        jit.emit_cmp_mem8_imm8(0, 8, 0);
        jit.emit_jz(is_false_lbl);

        jit.emit_mov_reg_imm64(1, 1);
        jit.emit_mov_mem_reg(0, 0, 1);
        jit.emit_mov_reg_imm64(1, 0);
        jit.emit_mov_mem_reg(0, 8, 1);
        jit.emit_jmp(end_lbl);

        jit.bind(is_false_lbl);
        jit.emit_mov_reg_imm64(1, 1);
        jit.emit_mov_mem_reg(0, 0, 1);
        jit.emit_mov_mem_reg(0, 8, 1);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_ADD:
    TARGET_OP_SUBTRACT:
    TARGET_OP_MULTIPLY:
    TARGET_OP_DIVIDE: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);

        uint8_t instruction = chunk->code[offset];
        if (instruction == OP_ADD) jit.emit_addsd_xmm_xmm(0, 1);
        else if (instruction == OP_SUBTRACT) jit.emit_subsd_xmm_xmm(0, 1);
        else if (instruction == OP_MULTIPLY) jit.emit_mulsd_xmm_xmm(0, 1);
        else if (instruction == OP_DIVIDE) jit.emit_divsd_xmm_xmm(0, 1);

        jit.emit_movsd_mem_xmm(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, instruction);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_MODULO: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);
        jit.emit_cvttsd2si_reg_xmm(0, 0);
        jit.emit_cvttsd2si_reg_xmm(1, 1);
        
        jit.emit_mov_reg_reg(0, 0);
        jit.emit_cqo();
        jit.emit_idiv_reg(1);
        jit.emit_mov_reg_reg(0, 2);
        
        jit.emit_cvtsi2sd_xmm_reg(0, 0);
        jit.emit_movsd_mem_xmm(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_MODULO);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_NEGATE: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_xorpd_xmm_xmm(1, 1);
        jit.emit_movsd_xmm_mem(0, 0, 8);
        jit.emit_subsd_xmm_xmm(1, 0);
        jit.emit_movsd_mem_xmm(0, 8, 1);
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_NEGATE);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_BITWISE_AND:
    TARGET_OP_BITWISE_OR:
    TARGET_OP_BITWISE_XOR: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);
        jit.emit_cvttsd2si_reg_xmm(0, 0);
        jit.emit_cvttsd2si_reg_xmm(1, 1);

        uint8_t instruction = chunk->code[offset];
        if (instruction == OP_BITWISE_AND) jit.emit_and_reg_reg(0, 1);
        else if (instruction == OP_BITWISE_OR) jit.emit_or_reg_reg(0, 1);
        else if (instruction == OP_BITWISE_XOR) jit.emit_xor_reg_reg(0, 1);

        jit.emit_cvtsi2sd_xmm_reg(0, 0);
        jit.emit_movsd_mem_xmm(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, instruction);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_LEFT_SHIFT:
    TARGET_OP_RIGHT_SHIFT: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);
        jit.emit_cvttsd2si_reg_xmm(0, 0);
        jit.emit_cvttsd2si_reg_xmm(1, 1);

        jit.emit_mov_reg_reg(1, 1);
        uint8_t instruction = chunk->code[offset];
        if (instruction == OP_LEFT_SHIFT) jit.emit_shl_reg_cl(0);
        else if (instruction == OP_RIGHT_SHIFT) jit.emit_sar_reg_cl(0);

        jit.emit_cvtsi2sd_xmm_reg(0, 0);
        jit.emit_movsd_mem_xmm(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, instruction);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_BITWISE_NOT: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 0, 8);
        jit.emit_cvttsd2si_reg_xmm(0, 0);
        jit.emit_not_reg(0);
        jit.emit_cvtsi2sd_xmm_reg(0, 0);
        jit.emit_movsd_mem_xmm(0, 8, 0);
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_BITWISE_NOT);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_JUMP: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        size_t target = offset + 3 + jump;
        if (target < labels.size()) {
            jit.emit_jmp(labels[target]);
        }
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_JUMP_IF_FALSE: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        size_t target = offset + 3 + jump;
        JitAssembler::Label is_false_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 0);
        jit.emit_jz(is_false_lbl);

        jit.emit_cmp_mem8_imm8(0, 0, 1);
        jit.emit_jnz(end_lbl);

        jit.emit_cmp_mem8_imm8(0, 8, 0);
        jit.emit_jz(is_false_lbl);
        jit.emit_jmp(end_lbl);

        jit.bind(is_false_lbl);
        if (target < labels.size()) {
            jit.emit_jmp(labels[target]);
        }

        jit.bind(end_lbl);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_JUMP_IF_NIL: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        size_t target = offset + 3 + jump;
        JitAssembler::Label not_nil_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 0);
        jit.emit_jnz(not_nil_lbl);

        if (target < labels.size()) {
            jit.emit_jmp(labels[target]);
        }

        jit.bind(not_nil_lbl);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        jit.bind(end_lbl);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_JUMP_IF_NOT_NIL: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        size_t target = offset + 3 + jump;
        JitAssembler::Label is_nil_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 0);
        jit.emit_jz(is_nil_lbl);

        if (target < labels.size()) {
            jit.emit_jmp(labels[target]);
        }

        jit.bind(is_nil_lbl);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        jit.bind(end_lbl);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_LOOP: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        // FIX v1.1.0: guard against underflow on backward jump
        if (jump <= offset + 3) {
            size_t target = offset + 3 - jump;
            if (target < labels.size()) {
                jit.emit_jmp(labels[target]);
            }
        }
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_CALL: {
        uint8_t arg_count = chunk->code[offset + 1];
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue) * (arg_count + 1));
        
        JitAssembler::Label is_closure_lbl, is_native_lbl, is_class_lbl, end_lbl;
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(end_lbl);
        
        jit.emit_mov_reg_mem(1, 0, 8 + offsetof(Obj, type));
        jit.emit_cmp_reg_imm32(1, OBJ_CLOSURE);
        jit.emit_jz(is_closure_lbl);
        jit.emit_cmp_reg_imm32(1, OBJ_NATIVE);
        jit.emit_jz(is_native_lbl);
        jit.emit_cmp_reg_imm32(1, OBJ_CLASS);
        jit.emit_jz(is_class_lbl);
        jit.emit_jmp(end_lbl);
        
        jit.bind(is_closure_lbl);
        emit_trampoline((void*)&jit_trampoline_call);
        jit.emit_jmp(end_lbl);
        
        jit.bind(is_native_lbl);
        emit_trampoline((void*)&jit_trampoline_call);
        jit.emit_jmp(end_lbl);
        
        jit.bind(is_class_lbl);
        emit_trampoline((void*)&jit_trampoline_call);
        
        jit.bind(end_lbl);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_CLOSURE: {
        emit_trampoline((void*)&jit_trampoline_closure);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_RETURN: {
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_pop_reg(15);
        jit.emit_pop_reg(14);
        jit.emit_pop_reg(13);
        jit.emit_pop_reg(12);
        jit.emit_pop_reg(7);
        jit.emit_pop_reg(6);
        jit.emit_pop_reg(5);
        jit.emit_pop_reg(3);
        jit.emit_ret();
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_IMPORT: {
        emit_trampoline((void*)&jit_trampoline_import);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_MAKE_NAMED_ARG: {
        emit_trampoline((void*)&jit_trampoline_make_named_arg);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_INHERIT: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_INHERIT);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_SUPER: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_GET_SUPER);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SPAWN: {
        emit_trampoline((void*)&jit_trampoline_spawn);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_AWAIT: {
        emit_trampoline((void*)&jit_trampoline_await);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_ASYNC_CALL: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_ASYNC_CALL);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_ITERATOR: {
        JitAssembler::Label is_array_lbl, end_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(end_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_ARRAY);
        jit.emit_jnz(end_lbl);
        
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.emit_mov_reg_imm64(0, 2);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_ITER_NEXT_IN: {
        JitAssembler::Label is_array_lbl, end_lbl, has_more_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue) * 2);
        jit.emit_mov_reg_reg(1, 12);
        jit.emit_sub_reg_imm32(1, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(end_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_ARRAY);
        jit.emit_jnz(end_lbl);
        
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_ITER_NEXT_IN);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_ITER_NEXT_OF: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_ITER_NEXT_OF);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_TRY_START: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        jit.emit_mov_reg_imm64(0, offset + 3 + jump);
        jit.emit_mov_mem_reg(13, offsetof(VM, catch_count), 0);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_TRY_END: {
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(13, offsetof(VM, catch_count), 0);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_THROW: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_THROW);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_WITHIN_START: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        jit.emit_jmp(labels[offset + 3 + jump]);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_WITHIN_END: {
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_EVERY_TICK: {
        uint32_t ms = (chunk->code[offset + 1] << 24) | (chunk->code[offset + 2] << 16) | 
                      (chunk->code[offset + 3] << 8) | chunk->code[offset + 4];
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, ms);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        offset += 5;
        goto NEXT_OPCODE;
    }

    TARGET_OP_UNDO: {
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_DEFINE_FADE: {
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_PRINT: {
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue)); 
        jit.emit_mov_reg_reg(1, 0); 
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_print_value); 
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SUPER: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_SUPER);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_THIS: {
        jit.emit_mov_reg_imm64(0, (uint64_t)&frame->slots[0]);
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_CLASS: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_CLASS);
        offset++;
        goto NEXT_OPCODE;
    }

JIT_END:
    typedef void (*JitFunc)();
    int jit_error = 0;
    std::string jit_error_msg;
    
    if (rubellite_debug) {
        printf("[JIT DEBUG] Finalizing JIT code (code_size=%zu)\n", jit.code_size());
    }
    
    JitFunc func = (JitFunc)jit.finalize(&jit_error, &jit_error_msg);

    if (func) {
        if (rubellite_debug) {
            printf("[JIT DEBUG] JIT code finalized successfully. Executing...\n");
        }
        func();
        if (rubellite_debug) {
            printf("[JIT DEBUG] JIT execution completed.\n");
        }
    } else {
        // FIX v1.1.0: fallback to interpreter instead of hard failing.
        // The JIT may fail on platforms without executable memory or on edge-case
        // bytecode sequences. The interpreter is always correct.
        if (rubellite_debug || !soft_mode) {
            printf("[JIT] Warning: JIT compilation failed (error=%d, code_size=%zu). "
                   "Falling back to interpreter.\n", jit_error, jit.code_size());
            if (!jit_error_msg.empty()) {
                printf("[JIT] Details: %s\n", jit_error_msg.c_str());
            }
        }
        // Reset instruction pointer to beginning of the frame's chunk and interpret
        frame->ip = frame->function->chunk.code.data();
        return vm_interpreter_fallback(this, target_frame_count);
    }

    return true;
}

