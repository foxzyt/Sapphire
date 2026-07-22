#ifndef RUBELLITE_JIT_ASSEMBLER_H
#define RUBELLITE_JIT_ASSEMBLER_H

#define ASMJIT_STATIC 1
#define ASMJIT_API

#include <asmjit/x86.h>
#include <asmjit/core.h>
#include <memory>
#include <stdexcept>
#include <cstdint>

class JitAssembler {
public:
    JitAssembler()
        : m_codeHolder(),
          m_runtime(std::make_unique<asmjit::JitRuntime>())
    {
        m_env.init(asmjit::Arch::kX86);
        m_codeHolder.init(m_env);
        m_assembler = std::make_unique<asmjit::x86::Assembler>(&m_codeHolder);
    }

    ~JitAssembler() {
        free_memory();
    }

    asmjit::x86::Assembler& asm_() { return *m_assembler; }

    void emit8(uint8_t b) { m_assembler->db(b); }
    void emit16(uint16_t w) { m_assembler->dw(w); }
    void emit32(uint32_t d) { m_assembler->dd(d); }
    void emit64(uint64_t q) { m_assembler->dq(q); }
    void emit_ret() { m_assembler->ret(); }

    void emit_push_reg(uint8_t reg) {
        m_assembler->push(asmjit::x86::gpq(reg));
    }

    void emit_pop_reg(uint8_t reg) {
        m_assembler->pop(asmjit::x86::gpq(reg));
    }

    void emit_mov_reg_imm64(uint8_t reg, uint64_t imm) {
        m_assembler->mov(asmjit::x86::gpq(reg), imm);
    }

    void emit_call_reg(uint8_t reg) {
        m_assembler->call(asmjit::x86::gpq(reg));
    }

    void emit_mov_reg_mem(uint8_t dst, uint8_t base, int32_t offset) {
        auto mem = (offset == 0)
            ? asmjit::x86::qword_ptr(asmjit::x86::gpq(base))
            : asmjit::x86::qword_ptr(asmjit::x86::gpq(base), offset);
        m_assembler->mov(asmjit::x86::gpq(dst), mem);
    }

    void emit_mov_mem_reg(uint8_t base, int32_t offset, uint8_t src) {
        auto mem = (offset == 0)
            ? asmjit::x86::qword_ptr(asmjit::x86::gpq(base))
            : asmjit::x86::qword_ptr(asmjit::x86::gpq(base), offset);
        m_assembler->mov(mem, asmjit::x86::gpq(src));
    }

    void emit_add_reg_reg(uint8_t dst, uint8_t src) {
        m_assembler->add(asmjit::x86::gpq(dst), asmjit::x86::gpq(src));
    }

    void emit_sub_reg_reg(uint8_t dst, uint8_t src) {
        m_assembler->sub(asmjit::x86::gpq(dst), asmjit::x86::gpq(src));
    }

    void emit_cmp_reg_reg(uint8_t dst, uint8_t src) {
        m_assembler->cmp(asmjit::x86::gpq(dst), asmjit::x86::gpq(src));
    }

    void emit_cmp_reg_imm32(uint8_t reg, int32_t imm) {
        m_assembler->cmp(asmjit::x86::gpq(reg), imm);
    }

    void emit_add_reg_imm32(uint8_t reg, int32_t imm) {
        m_assembler->add(asmjit::x86::gpq(reg), imm);
    }

    void emit_sub_reg_imm32(uint8_t reg, int32_t imm) {
        m_assembler->sub(asmjit::x86::gpq(reg), imm);
    }

    void emit_cmp_mem8_imm8(uint8_t base, int32_t offset, uint8_t imm) {
        auto mem = (offset == 0)
            ? asmjit::x86::byte_ptr(asmjit::x86::gpq(base))
            : asmjit::x86::byte_ptr(asmjit::x86::gpq(base), offset);
        m_assembler->cmp(mem, imm);
    }

    void emit_movsd_xmm_mem(uint8_t xmm_dst, uint8_t base, int32_t offset) {
        auto mem = (offset == 0)
            ? asmjit::x86::qword_ptr(asmjit::x86::gpq(base))
            : asmjit::x86::qword_ptr(asmjit::x86::gpq(base), offset);
        m_assembler->movsd(asmjit::x86::xmm(xmm_dst), mem);
    }

    void emit_movsd_mem_xmm(uint8_t base, int32_t offset, uint8_t xmm_src) {
        auto mem = (offset == 0)
            ? asmjit::x86::qword_ptr(asmjit::x86::gpq(base))
            : asmjit::x86::qword_ptr(asmjit::x86::gpq(base), offset);
        m_assembler->movsd(mem, asmjit::x86::xmm(xmm_src));
    }

    void emit_addsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        m_assembler->addsd(asmjit::x86::xmm(xmm_dst), asmjit::x86::xmm(xmm_src));
    }

    void emit_subsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        m_assembler->subsd(asmjit::x86::xmm(xmm_dst), asmjit::x86::xmm(xmm_src));
    }

    void emit_mulsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        m_assembler->mulsd(asmjit::x86::xmm(xmm_dst), asmjit::x86::xmm(xmm_src));
    }

    void emit_divsd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        m_assembler->divsd(asmjit::x86::xmm(xmm_dst), asmjit::x86::xmm(xmm_src));
    }

    void emit_ucomisd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        m_assembler->ucomisd(asmjit::x86::xmm(xmm_dst), asmjit::x86::xmm(xmm_src));
    }

    void emit_xorpd_xmm_xmm(uint8_t xmm_dst, uint8_t xmm_src) {
        m_assembler->xorpd(asmjit::x86::xmm(xmm_dst), asmjit::x86::xmm(xmm_src));
    }

    void emit_cvttsd2si_reg_xmm(uint8_t dst_reg, uint8_t src_xmm) {
        m_assembler->cvttsd2si(asmjit::x86::gpq(dst_reg), asmjit::x86::xmm(src_xmm));
    }

    void emit_cvtsi2sd_xmm_reg(uint8_t dst_xmm, uint8_t src_reg) {
        m_assembler->cvtsi2sd(asmjit::x86::xmm(dst_xmm), asmjit::x86::gpq(src_reg));
    }

    void emit_and_reg_imm32(uint8_t reg, int32_t imm) {
        m_assembler->and_(asmjit::x86::gpq(reg), imm);
    }

    void emit_and_reg_reg(uint8_t dst, uint8_t src) {
        m_assembler->and_(asmjit::x86::gpq(dst), asmjit::x86::gpq(src));
    }

    void emit_or_reg_reg(uint8_t dst, uint8_t src) {
        m_assembler->or_(asmjit::x86::gpq(dst), asmjit::x86::gpq(src));
    }

    void emit_xor_reg_reg(uint8_t dst, uint8_t src) {
        m_assembler->xor_(asmjit::x86::gpq(dst), asmjit::x86::gpq(src));
    }

    void emit_not_reg(uint8_t reg) {
        m_assembler->not_(asmjit::x86::gpq(reg));
    }

    void emit_shl_reg_cl(uint8_t reg) {
        m_assembler->shl(asmjit::x86::gpq(reg), asmjit::x86::cl);
    }

    void emit_sar_reg_cl(uint8_t reg) {
        m_assembler->sar(asmjit::x86::gpq(reg), asmjit::x86::cl);
    }

    void emit_cqo() {
        m_assembler->cqo();
    }

    void emit_idiv_reg(uint8_t reg) {
        m_assembler->idiv(asmjit::x86::gpq(reg));
    }

    void emit_movzx_reg64_reg8(uint8_t dst, uint8_t src) {
        m_assembler->movzx(asmjit::x86::gpq(dst), asmjit::x86::gpb(src));
    }

    void emit_mov_reg_reg(uint8_t dst, uint8_t src) {
        m_assembler->mov(asmjit::x86::gpq(dst), asmjit::x86::gpq(src));
    }

    using Label = asmjit::Label;

    Label new_label() {
        return m_assembler->new_label();
    }

    void bind(Label& label) {
        m_assembler->bind(label);
    }

    void emit_jmp(Label& label) {
        m_assembler->jmp(label);
    }

    enum CC {
        CC_E  = 0x4,
        CC_NE = 0x5,
        CC_B  = 0x2,
        CC_AE = 0x3,
        CC_A  = 0x7,
        CC_BE = 0x6
    };

    void emit_jcc(CC cc, Label& label) {
        switch (cc) {
            case CC_E:  m_assembler->je(label);  break;
            case CC_NE: m_assembler->jne(label); break;
            case CC_B:  m_assembler->jb(label);  break;
            case CC_AE: m_assembler->jae(label); break;
            case CC_A:  m_assembler->ja(label);  break;
            case CC_BE: m_assembler->jbe(label); break;
        }
    }

    void emit_jz(Label& label)  { m_assembler->je(label); }
    void emit_jnz(Label& label) { m_assembler->jne(label); }
    void emit_ja(Label& label)  { m_assembler->ja(label); }
    void emit_jb(Label& label)  { m_assembler->jb(label); }
    void emit_jae(Label& label) { m_assembler->jae(label); }
    void emit_jbe(Label& label) { m_assembler->jbe(label); }

    void emit_setcc(CC cc, uint8_t reg) {
        switch (cc) {
            case CC_E:  m_assembler->sete(asmjit::x86::gpb(reg));  break;
            case CC_NE: m_assembler->setne(asmjit::x86::gpb(reg)); break;
            case CC_A:  m_assembler->seta(asmjit::x86::gpb(reg));  break;
            case CC_B:  m_assembler->setb(asmjit::x86::gpb(reg));  break;
        }
    }

    void emit_sete(uint8_t reg)  { m_assembler->sete(asmjit::x86::gpb(reg)); }
    void emit_setne(uint8_t reg) { m_assembler->setne(asmjit::x86::gpb(reg)); }
    void emit_seta(uint8_t reg)  { m_assembler->seta(asmjit::x86::gpb(reg)); }
    void emit_setb(uint8_t reg)  { m_assembler->setb(asmjit::x86::gpb(reg)); }

    void emit_sub_reg_imm64(uint8_t reg, int64_t imm) {
        if (imm == 0) return;
        if (imm > 0 && imm < 0x80000000) {
            m_assembler->sub(asmjit::x86::gpq(reg), (int32_t)imm);
        } else {
            m_assembler->mov(asmjit::x86::gpq(15), imm);
            m_assembler->sub(asmjit::x86::gpq(reg), asmjit::x86::gpq(15));
        }
    }

    void emit_add_reg_imm64(uint8_t reg, int64_t imm) {
        if (imm == 0) return;
        if (imm > 0 && imm < 0x80000000) {
            m_assembler->add(asmjit::x86::gpq(reg), (int32_t)imm);
        } else {
            m_assembler->mov(asmjit::x86::gpq(15), imm);
            m_assembler->add(asmjit::x86::gpq(reg), asmjit::x86::gpq(15));
        }
    }

    void emit_mov_reg_mem_offset(uint8_t dst, uint8_t base, int32_t offset, uint8_t size_bytes) {
        if (size_bytes == 1) {
            auto mem = (offset == 0) ? asmjit::x86::byte_ptr(asmjit::x86::gpq(base))
                                      : asmjit::x86::byte_ptr(asmjit::x86::gpq(base), offset);
            m_assembler->movzx(asmjit::x86::gpq(dst), mem);
        } else if (size_bytes == 2) {
            auto mem = (offset == 0) ? asmjit::x86::word_ptr(asmjit::x86::gpq(base))
                                      : asmjit::x86::word_ptr(asmjit::x86::gpq(base), offset);
            m_assembler->movzx(asmjit::x86::gpq(dst), mem);
        } else if (size_bytes == 4) {
            auto mem = (offset == 0) ? asmjit::x86::dword_ptr(asmjit::x86::gpq(base))
                                      : asmjit::x86::dword_ptr(asmjit::x86::gpq(base), offset);
            m_assembler->mov(asmjit::x86::gpq(dst), mem);
        } else if (size_bytes == 8) {
            auto mem = (offset == 0) ? asmjit::x86::qword_ptr(asmjit::x86::gpq(base))
                                      : asmjit::x86::qword_ptr(asmjit::x86::gpq(base), offset);
            m_assembler->mov(asmjit::x86::gpq(dst), mem);
        }
    }

    void emit_mov_mem_reg_offset(uint8_t base, int32_t offset, uint8_t src, uint8_t size_bytes) {
        if (size_bytes == 1) {
            auto mem = (offset == 0) ? asmjit::x86::byte_ptr(asmjit::x86::gpq(base))
                                      : asmjit::x86::byte_ptr(asmjit::x86::gpq(base), offset);
            m_assembler->mov(mem, asmjit::x86::gpb(src));
        } else if (size_bytes == 2) {
            auto mem = (offset == 0) ? asmjit::x86::word_ptr(asmjit::x86::gpq(base))
                                      : asmjit::x86::word_ptr(asmjit::x86::gpq(base), offset);
            m_assembler->mov(mem, asmjit::x86::gpw(src));
        } else if (size_bytes == 4) {
            auto mem = (offset == 0) ? asmjit::x86::dword_ptr(asmjit::x86::gpq(base))
                                      : asmjit::x86::dword_ptr(asmjit::x86::gpq(base), offset);
            m_assembler->mov(mem, asmjit::x86::gpd(src));
        } else if (size_bytes == 8) {
            auto mem = (offset == 0) ? asmjit::x86::qword_ptr(asmjit::x86::gpq(base))
                                      : asmjit::x86::qword_ptr(asmjit::x86::gpq(base), offset);
            m_assembler->mov(mem, asmjit::x86::gpq(src));
        }
    }

    void emit_imul_reg_imm32(uint8_t reg, int32_t imm) {
        m_assembler->imul(asmjit::x86::gpq(reg), asmjit::x86::gpq(reg), imm);
    }

    void emit_imul_reg_imm32(uint8_t reg, uint8_t src, int32_t imm) {
        m_assembler->imul(asmjit::x86::gpq(reg), asmjit::x86::gpq(src), imm);
    }

    void* finalize(int* out_error = nullptr, std::string* out_error_msg = nullptr) {
        if (m_codeHolder.code_size() == 0) {
            if (out_error) *out_error = 1;
            if (out_error_msg) *out_error_msg = "No JIT code was generated (code_size = 0)";
            return nullptr;
        }

        if (m_jitEntry) {
            m_runtime->release(m_jitEntry);
            m_jitEntry = nullptr;
        }

        asmjit::Error err = m_runtime->add(&m_jitEntry, &m_codeHolder);
        if (err != asmjit::kErrorOk) {
            m_jitEntry = nullptr;
            if (out_error) *out_error = 2;
            if (out_error_msg) {
                *out_error_msg = std::string("AsmJit error ") + std::to_string((int)err);
            }
            return nullptr;
        }
        if (out_error) *out_error = 0;
        if (out_error_msg) *out_error_msg = "";
        return m_jitEntry;
    }

    size_t code_size() const {
        return m_codeHolder.code_size();
    }

    void free_memory() {
        if (m_jitEntry) {
            m_runtime->release(m_jitEntry);
            m_jitEntry = nullptr;
        }
    }

private:
    asmjit::Environment m_env;
    asmjit::CodeHolder m_codeHolder;
    std::unique_ptr<asmjit::JitRuntime> m_runtime;
    std::unique_ptr<asmjit::x86::Assembler> m_assembler;
    void* m_jitEntry = nullptr;
};

#endif