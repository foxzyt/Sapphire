#ifndef RUBELLITE_JIT_ASSEMBLER_H
#define RUBELLITE_JIT_ASSEMBLER_H

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

class JitAssembler {
public:
    JitAssembler() {}
    
    ~JitAssembler() {
        free_memory();
    }

    void emit8(uint8_t b) {
        code.push_back(b);
    }

    void emit16(uint16_t w) {
        code.push_back(w & 0xFF);
        code.push_back((w >> 8) & 0xFF);
    }

    void emit32(uint32_t d) {
        code.push_back(d & 0xFF);
        code.push_back((d >> 8) & 0xFF);
        code.push_back((d >> 16) & 0xFF);
        code.push_back((d >> 24) & 0xFF);
    }

    void emit64(uint64_t q) {
        emit32(q & 0xFFFFFFFF);
        emit32((q >> 32) & 0xFFFFFFFF);
    }

    // x86-64 basics
    void emit_ret() {
        emit8(0xC3);
    }

    void emit_push_reg(uint8_t reg) {
        // reg: 0=rax, 1=rcx, 2=rdx, 3=rbx, 4=rsp, 5=rbp, 6=rsi, 7=rdi
        emit8(0x50 + (reg & 7));
    }

    void emit_pop_reg(uint8_t reg) {
        emit8(0x58 + (reg & 7));
    }

    // mov reg64, imm64
    void emit_mov_reg_imm64(uint8_t reg, uint64_t imm) {
        // REX.W + B8+rd
        uint8_t rex = 0x48 | ((reg >> 3) & 1);
        emit8(rex);
        emit8(0xB8 + (reg & 7));
        emit64(imm);
    }

    // call reg64
    void emit_call_reg(uint8_t reg) {
        uint8_t rex = 0x40 | ((reg >> 3) & 1);
        if (rex != 0x40) emit8(rex);
        emit8(0xFF);
        emit8(0xD0 + (reg & 7));
    }

    struct Label {
        size_t target_offset = 0;
        bool bound = false;
        std::vector<size_t> unresolved_jumps;
    };

    void bind(Label& label) {
        label.target_offset = code.size();
        label.bound = true;
        // Backpatch all jumps that referenced this label
        for (size_t jump_offset : label.unresolved_jumps) {
            // jump_offset points to the 4-byte relative offset
            int32_t rel = (int32_t)label.target_offset - (int32_t)(jump_offset + 4);
            code[jump_offset] = rel & 0xFF;
            code[jump_offset + 1] = (rel >> 8) & 0xFF;
            code[jump_offset + 2] = (rel >> 16) & 0xFF;
            code[jump_offset + 3] = (rel >> 24) & 0xFF;
        }
        label.unresolved_jumps.clear();
    }

    void emit_jmp(Label& label) {
        emit8(0xE9);
        if (label.bound) {
            int32_t rel = (int32_t)label.target_offset - (int32_t)(code.size() + 4);
            emit32(rel);
        } else {
            label.unresolved_jumps.push_back(code.size());
            emit32(0);
        }
    }

    // Condition codes
    enum CC {
        CC_E = 0x4, CC_NE = 0x5,
        CC_B = 0x2, CC_AE = 0x3,
        CC_A = 0x7, CC_BE = 0x6
    };

    void emit_jcc(CC cc, Label& label) {
        emit8(0x0F);
        emit8(0x80 | cc);
        if (label.bound) {
            int32_t rel = (int32_t)label.target_offset - (int32_t)(code.size() + 4);
            emit32(rel);
        } else {
            label.unresolved_jumps.push_back(code.size());
            emit32(0);
        }
    }
    
    void emit_jz(Label& label) { emit_jcc(CC_E, label); }
    void emit_jnz(Label& label) { emit_jcc(CC_NE, label); }
    void emit_ja(Label& label) { emit_jcc(CC_A, label); }
    void emit_jb(Label& label) { emit_jcc(CC_B, label); }
    void emit_jae(Label& label) { emit_jcc(CC_AE, label); }
    void emit_jbe(Label& label) { emit_jcc(CC_BE, label); }

    void emit_setcc(CC cc, uint8_t reg) {
        // setcc r/m8
        uint8_t rex = 0x40 | (reg >> 3);
        if (rex != 0x40 || (reg & 7) >= 4) emit8(rex); // requires REX if reg >= 4 (spl, bpl, etc)
        emit8(0x0F);
        emit8(0x90 | cc);
        emit8(0xC0 | (reg & 7));
    }
    
    void emit_sete(uint8_t reg) { emit_setcc(CC_E, reg); }
    void emit_setne(uint8_t reg) { emit_setcc(CC_NE, reg); }
    void emit_seta(uint8_t reg) { emit_setcc(CC_A, reg); }
    void emit_setb(uint8_t reg) { emit_setcc(CC_B, reg); }

    // movzx dst64, src8 (zero extend byte to 64-bit)
    void emit_movzx_reg64_reg8(uint8_t dst, uint8_t src) {
        uint8_t rex = 0x48 | ((dst >> 3) << 2) | (src >> 3);
        emit8(rex);
        emit8(0x0F);
        emit8(0xB6);
        emit8(0xC0 | ((dst & 7) << 3) | (src & 7));
    }

    // mov dst, src (64-bit registers)
    void emit_mov_reg_reg(uint8_t dst, uint8_t src) {
        uint8_t rex = 0x48 | ((src >> 3) << 2) | (dst >> 3);
        emit8(rex);
        emit8(0x89);
        emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    // mov dst, [base + offset]
    void emit_mov_reg_mem(uint8_t dst, uint8_t base, int32_t offset) {
        uint8_t rex = 0x48 | ((dst >> 3) << 2) | (base >> 3);
        emit8(rex);
        emit8(0x8B);
        if (offset >= -128 && offset <= 127) {
            emit8(0x40 | ((dst & 7) << 3) | (base & 7));
            emit8((uint8_t)offset);
        } else {
            emit8(0x80 | ((dst & 7) << 3) | (base & 7));
            emit32(offset);
        }
    }

    // mov [base + offset], src
    void emit_mov_mem_reg(uint8_t base, int32_t offset, uint8_t src) {
        uint8_t rex = 0x48 | ((src >> 3) << 2) | (base >> 3);
        emit8(rex);
        emit8(0x89);
        if (offset >= -128 && offset <= 127) {
            emit8(0x40 | ((src & 7) << 3) | (base & 7));
            emit8((uint8_t)offset);
        } else {
            emit8(0x80 | ((src & 7) << 3) | (base & 7));
            emit32(offset);
        }
    }

    // add dst, src
    void emit_add_reg_reg(uint8_t dst, uint8_t src) {
        uint8_t rex = 0x48 | ((src >> 3) << 2) | (dst >> 3);
        emit8(rex);
        emit8(0x01);
        emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    // sub dst, src
    void emit_sub_reg_reg(uint8_t dst, uint8_t src) {
        uint8_t rex = 0x48 | ((src >> 3) << 2) | (dst >> 3);
        emit8(rex);
        emit8(0x29);
        emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }
    
    // cmp dst, src
    void emit_cmp_reg_reg(uint8_t dst, uint8_t src) {
        uint8_t rex = 0x48 | ((src >> 3) << 2) | (dst >> 3);
        emit8(rex);
        emit8(0x39);
        emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }
    
    // cmp reg, imm32
    void emit_cmp_reg_imm32(uint8_t reg, int32_t imm) {
        uint8_t rex = 0x48 | (reg >> 3);
        emit8(rex);
        if (imm >= -128 && imm <= 127) {
            emit8(0x83);
            emit8(0xF8 | (reg & 7));
            emit8((uint8_t)imm);
        } else {
            emit8(0x81);
            emit8(0xF8 | (reg & 7));
            emit32(imm);
        }
    }

    // add reg, imm32
    void emit_add_reg_imm32(uint8_t reg, int32_t imm) {
        uint8_t rex = 0x48 | (reg >> 3);
        emit8(rex);
        if (imm >= -128 && imm <= 127) {
            emit8(0x83);
            emit8(0xC0 | (reg & 7));
            emit8((uint8_t)imm);
        } else {
            emit8(0x81);
            emit8(0xC0 | (reg & 7));
            emit32(imm);
        }
    }

    // sub reg, imm32
    void emit_sub_reg_imm32(uint8_t reg, int32_t imm) {
        uint8_t rex = 0x48 | (reg >> 3);
        emit8(rex);
        if (imm >= -128 && imm <= 127) {
            emit8(0x83);
            emit8(0xE8 | (reg & 7));
            emit8((uint8_t)imm);
        } else {
            emit8(0x81);
            emit8(0xE8 | (reg & 7));
            emit32(imm);
        }
    }

    // cmp byte [base + offset], imm8
    void emit_cmp_mem8_imm8(uint8_t base, int32_t offset, uint8_t imm) {
        uint8_t rex = 0x40 | (base >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x80);
        if (offset >= -128 && offset <= 127) {
            emit8(0x78 | (base & 7));
            emit8((uint8_t)offset);
        } else {
            emit8(0xB8 | (base & 7));
            emit32(offset);
        }
        emit8(imm);
    }

    // SSE2 Instructions
    
    // movsd xmm_dst, [base + offset]
    void emit_movsd_xmm_mem(uint8_t xmm_dst, uint8_t base, int32_t offset) {
        emit8(0xF2);
        uint8_t rex = 0x40 | ((xmm_dst >> 3) << 2) | (base >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x0F);
        emit8(0x10);
        if (offset >= -128 && offset <= 127) {
            emit8(0x40 | ((xmm_dst & 7) << 3) | (base & 7));
            emit8((uint8_t)offset);
        } else {
            emit8(0x80 | ((xmm_dst & 7) << 3) | (base & 7));
            emit32(offset);
        }
    }

    // movsd [base + offset], xmm_src
    void emit_movsd_mem_xmm(uint8_t base, int32_t offset, uint8_t xmm_src) {
        emit8(0xF2);
        uint8_t rex = 0x40 | ((xmm_src >> 3) << 2) | (base >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x0F);
        emit8(0x11);
        if (offset >= -128 && offset <= 127) {
            emit8(0x40 | ((xmm_src & 7) << 3) | (base & 7));
            emit8((uint8_t)offset);
        } else {
            emit8(0x80 | ((xmm_src & 7) << 3) | (base & 7));
            emit32(offset);
        }
    }

    // addsd xmm_dst, xmm_src
    void emit_addsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        emit8(0xF2);
        uint8_t rex = 0x40 | ((xmm_dst >> 3) << 2) | (xmm_src >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x0F);
        emit8(0x58);
        emit8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // subsd xmm_dst, xmm_src
    void emit_subsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        emit8(0xF2);
        uint8_t rex = 0x40 | ((xmm_dst >> 3) << 2) | (xmm_src >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x0F);
        emit8(0x5C);
        emit8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // mulsd xmm_dst, xmm_src
    void emit_mulsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        emit8(0xF2);
        uint8_t rex = 0x40 | ((xmm_dst >> 3) << 2) | (xmm_src >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x0F);
        emit8(0x59);
        emit8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // divsd xmm_dst, xmm_src
    void emit_divsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        emit8(0xF2);
        uint8_t rex = 0x40 | ((xmm_dst >> 3) << 2) | (xmm_src >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x0F);
        emit8(0x5E);
        emit8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // ucomisd xmm_dst, xmm_src (compare floats)
    void emit_ucomisd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        emit8(0x66);
        uint8_t rex = 0x40 | ((xmm_dst >> 3) << 2) | (xmm_src >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x0F);
        emit8(0x2E);
        emit8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // xorpd xmm_dst, xmm_src
    void emit_xorpd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        emit8(0x66);
        uint8_t rex = 0x40 | ((xmm_dst >> 3) << 2) | (xmm_src >> 3);
        if (rex != 0x40) emit8(rex);
        emit8(0x0F);
        emit8(0x57);
        emit8(0xC0 | ((xmm_dst & 7) << 3) | (xmm_src & 7));
    }

    // cvttsd2si reg64, xmm (Convert with Truncation Scalar Double to Integer)
    void emit_cvttsd2si_reg_xmm(uint8_t dst_reg, uint8_t src_xmm) {
        emit8(0xF2);
        uint8_t rex = 0x48 | ((dst_reg >> 3) << 2) | (src_xmm >> 3);
        emit8(rex);
        emit8(0x0F);
        emit8(0x2C);
        emit8(0xC0 | ((dst_reg & 7) << 3) | (src_xmm & 7));
    }

    // cvtsi2sd xmm, reg64 (Convert Integer to Scalar Double)
    void emit_cvtsi2sd_xmm_reg(uint8_t dst_xmm, uint8_t src_reg) {
        emit8(0xF2);
        uint8_t rex = 0x48 | ((dst_xmm >> 3) << 2) | (src_reg >> 3);
        emit8(rex);
        emit8(0x0F);
        emit8(0x2A);
        emit8(0xC0 | ((dst_xmm & 7) << 3) | (src_reg & 7));
    }

    // and dst, src (64-bit)
    void emit_and_reg_reg(uint8_t dst, uint8_t src) {
        uint8_t rex = 0x48 | ((src >> 3) << 2) | (dst >> 3);
        emit8(rex);
        emit8(0x21);
        emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    // or dst, src (64-bit)
    void emit_or_reg_reg(uint8_t dst, uint8_t src) {
        uint8_t rex = 0x48 | ((src >> 3) << 2) | (dst >> 3);
        emit8(rex);
        emit8(0x09);
        emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    // xor dst, src (64-bit)
    void emit_xor_reg_reg(uint8_t dst, uint8_t src) {
        uint8_t rex = 0x48 | ((src >> 3) << 2) | (dst >> 3);
        emit8(rex);
        emit8(0x31);
        emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    // not reg (64-bit)
    void emit_not_reg(uint8_t reg) {
        uint8_t rex = 0x48 | (reg >> 3);
        emit8(rex);
        emit8(0xF7);
        emit8(0xD0 | (reg & 7));
    }

    // shl reg, cl (64-bit)
    void emit_shl_reg_cl(uint8_t reg) {
        uint8_t rex = 0x48 | (reg >> 3);
        emit8(rex);
        emit8(0xD3);
        emit8(0xE0 | (reg & 7));
    }

    // sar reg, cl (64-bit, arithmetic right shift)
    void emit_sar_reg_cl(uint8_t reg) {
        uint8_t rex = 0x48 | (reg >> 3);
        emit8(rex);
        emit8(0xD3);
        emit8(0xF8 | (reg & 7));
    }

    // cqo (Sign-extend RAX into RDX:RAX)
    void emit_cqo() {
        emit8(0x48);
        emit8(0x99);
    }

    // idiv reg (64-bit, divides RDX:RAX by reg)
    void emit_idiv_reg(uint8_t reg) {
        uint8_t rex = 0x48 | (reg >> 3);
        emit8(rex);
        emit8(0xF7);
        emit8(0xF8 | (reg & 7));
    }

    void* finalize() {
        if (code.empty()) return nullptr;
        
        size_t size = code.size();
#ifdef _WIN32
        exec_mem = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        exec_mem = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (exec_mem == MAP_FAILED) exec_mem = nullptr;
#endif
        if (!exec_mem) {
            throw std::runtime_error("Failed to allocate JIT memory");
        }

        std::memcpy(exec_mem, code.data(), size);
        exec_size = size;

#ifdef _WIN32
        DWORD old_protect;
        VirtualProtect(exec_mem, size, PAGE_EXECUTE_READ, &old_protect);
#else
        mprotect(exec_mem, size, PROT_READ | PROT_EXEC);
#endif
        return exec_mem;
    }

    void free_memory() {
        if (exec_mem) {
#ifdef _WIN32
            VirtualFree(exec_mem, 0, MEM_RELEASE);
#else
            munmap(exec_mem, exec_size);
#endif
            exec_mem = nullptr;
        }
    }

private:
    std::vector<uint8_t> code;
    void* exec_mem = nullptr;
    size_t exec_size = 0;
};

#endif
