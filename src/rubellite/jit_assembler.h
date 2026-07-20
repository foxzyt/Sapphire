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
