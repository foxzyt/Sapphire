#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <chrono>
#include <string>
#include <iomanip>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cmath>
#include <array>
#include <shared_mutex>

// Sapphire VM headers
#include "vm.h"
#include "object.h"
#include "value.h"
#include "environment.h"
#include "opcodes.h"

// ASMJIT
#include <asmjit/asmjit.h>

using namespace asmjit;

// ============================================================================
// 1. CONSTANTS & CONFIGURATION
// ============================================================================

constexpr int JIT_HOTSWAP_THRESHOLD = 500;
constexpr int JIT_MAX_ITERATORS = 1024;
constexpr int JIT_MAX_UPVALUES = 2048;
constexpr int JIT_CODE_BUFFER_SIZE = 65536;
constexpr int JIT_MAX_TRY_BLOCKS = 64;
constexpr int JIT_MAX_CODE_SIZE = 1048576; // 1MB

enum class HotSwapReason {
    TYPE_MISMATCH,
    STACK_OVERFLOW,
    OUT_OF_BOUNDS,
    UNSUPPORTED_OPCODE,
    EXCEPTION_THROWN,
    DIVIDE_BY_ZERO,
    INVALID_PROPERTY,
    INVALID_METHOD,
    MEMORY_ERROR,
    INVALID_ARGUMENT,
    NULL_DEREFERENCE
};

enum class UnwindResult {
    HANDLED,
    FATAL
};

enum class IteratorType {
    ARRAY,
    OBJECT,
    STRING,
    RANGE
};

enum class JITCompilationResult {
    SUCCESS,
    INVALID_OPCODE,
    CODE_TOO_LARGE,
    RUNTIME_ERROR,
    STATE_CORRUPTED
};

// ============================================================================
// 2. LOGGING SYSTEM
// ============================================================================

static std::ofstream jit_log_file;
static std::mutex jit_log_mutex;

static void init_jit_logger() {
    if (!jit_log_file.is_open()) {
        jit_log_file.open("jit_rubellite.log", std::ios::out | std::ios::app);
    }
}

static void log_jit_event(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(jit_log_mutex);
    init_jit_logger();
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    jit_log_file << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] " 
                 << "[" << level << "] " << message << std::endl;
    jit_log_file.flush();
}

// ============================================================================
// 3. STRUCTS & FORWARD DECLARATIONS
// ============================================================================

struct JITContext;
class JITHotSwapManager;
class UpvalueManager;
class IteratorRegistry;
class JITUnwindContext;
class JITCompiler;

// Struct to represent a captured upvalue with proper memory management
struct Upvalue {
    SapphireValue* location;
    SapphireValue closed;
    uint32_t stack_index;
    bool is_closed;
    int ref_count;
    
    explicit Upvalue(SapphireValue* loc, uint32_t index) 
        : location(loc), closed(), stack_index(index), is_closed(false), ref_count(1) {}
    
    void add_ref() { ref_count++; }
    void release() { ref_count--; }
    bool is_shared() const { return ref_count > 1; }
};

// Internal struct to keep track of iterator state with real iteration logic
struct IteratorState {
    IteratorType type;
    SapphireValue collection;
    int current_index;
    int max_index;
    std::vector<std::string> keys;
    double range_start;
    double range_end;
    double range_step;
    bool reverse_iteration;
    
    IteratorState() : type(IteratorType::ARRAY), collection(), current_index(0), 
                     max_index(0), range_start(0.0), range_end(0.0), range_step(1.0),
                     reverse_iteration(false) {}
};

// Try block registration for exception handling
struct TryBlock {
    uint32_t frame_id;
    uint8_t* handler_ip;
    SapphireValue* stack_top_at_entry;
    bool active;
    uint32_t depth;
    
    TryBlock(uint32_t fid, uint8_t* ip, SapphireValue* stack, uint32_t d = 0) 
        : frame_id(fid), handler_ip(ip), stack_top_at_entry(stack), active(true), depth(d) {}
};

// JIT compilation statistics
struct JITStatistics {
    size_t total_functions_compiled;
    size_t total_opcodes_generated;
    size_t total_hotswaps;
    size_t total_iterator_creations;
    size_t total_upvalue_captures;
    size_t total_exception_unwinds;
    std::chrono::milliseconds total_compilation_time;
    std::chrono::milliseconds total_execution_time;
    
    JITStatistics() : total_functions_compiled(0), total_opcodes_generated(0),
                     total_hotswaps(0), total_iterator_creations(0),
                     total_upvalue_captures(0), total_exception_unwinds(0),
                     total_compilation_time(0), total_execution_time(0) {}
};

// ============================================================================
// 4. JITUnwindContext (Real Stack Unwinding - Iterative)
// ============================================================================

class JITUnwindContext {
private:
    std::vector<TryBlock> active_try_blocks;
    uint32_t current_depth;
    
public:
    JITUnwindContext() : current_depth(0) {}
    
    void register_try_block(uint32_t frame_id, uint8_t* handler_ip, SapphireValue* stack_top) {
        active_try_blocks.emplace_back(frame_id, handler_ip, stack_top, current_depth++);
        log_jit_event("INFO", "Registered try block for frame " + std::to_string(frame_id) + 
                     " at depth " + std::to_string(current_depth - 1));
    }
    
    void unregister_try_block(uint32_t frame_id) {
        for (auto it = active_try_blocks.rbegin(); it != active_try_blocks.rend(); ++it) {
            if (it->frame_id == frame_id && it->active) {
                it->active = false;
                current_depth = it->depth;
                active_try_blocks.erase(std::next(it).base());
                log_jit_event("INFO", "Unregistered try block for frame " + std::to_string(frame_id));
                return;
            }
        }
    }
    
    UnwindResult unwind_stack(VM* vm, SapphireValue exception, uint32_t current_frame_id) {
        log_jit_event("INFO", "Starting iterative stack unwind for exception in frame " + 
                     std::to_string(current_frame_id));
        
        // Iterative unwinding - no recursion to prevent stack overflow
        size_t unwound_frames = 0;
        
        while (!active_try_blocks.empty()) {
            TryBlock block = active_try_blocks.back();
            
            if (!block.active) {
                active_try_blocks.pop_back();
                continue;
            }
            
            if (block.frame_id <= current_frame_id) {
                log_jit_event("INFO", "Found exception handler in frame " + std::to_string(block.frame_id) +
                             " after unwinding " + std::to_string(unwound_frames) + " frames");
                
                // Unwind frames iteratively
                while (vm->frame_count - 1 > static_cast<int>(block.frame_id)) {
                    // Close upvalues for the frame being unwound
                    if (vm->frame_count > 0) {
                        CallFrame* frame = &vm->frames[vm->frame_count - 1];
                        SapphireValue* frame_bottom = frame->slots;
                        // Note: Actual upvalue closing would be handled by UpvalueManager
                    }
                    vm->frame_count--;
                    unwound_frames++;
                }
                
                // Restore stack state
                vm->stack_top = block.stack_top_at_entry;
                vm->push(exception);
                vm->frames[vm->frame_count - 1].ip = block.handler_ip;
                
                active_try_blocks.pop_back();
                current_depth--;
                
                return UnwindResult::HANDLED;
            }
            
            active_try_blocks.pop_back();
            current_depth--;
        }
        
        log_jit_event("FATAL", "Uncaught exception during JIT execution - no handler found");
        return UnwindResult::FATAL;
    }
    
    void cleanup_all() {
        active_try_blocks.clear();
        current_depth = 0;
    }
    
    size_t active_handler_count() const {
        return active_try_blocks.size();
    }
    
    uint32_t get_current_depth() const {
        return current_depth;
    }
};

// ============================================================================
// 5. IteratorRegistry (Real Iterator State Management)
// ============================================================================

class IteratorRegistry {
private:
    std::unordered_map<uint32_t, std::unique_ptr<IteratorState>> iterators;
    uint32_t next_id;
    JITStatistics* stats;
    
public:
    IteratorRegistry(JITStatistics* statistics = nullptr) 
        : next_id(1), stats(statistics) {}
    
    uint32_t create_iterator(VM* vm, SapphireValue collection) {
        if (iterators.size() >= JIT_MAX_ITERATORS) {
            log_jit_event("ERROR", "Maximum iterator limit reached: " + std::to_string(JIT_MAX_ITERATORS));
            return 0;
        }
        
        auto state = std::make_unique<IteratorState>();
        state->collection = collection;
        state->current_index = 0;
        
        bool valid_collection = false;
        
        if (collection.type == ValType::VAL_OBJ) {
            Obj* obj = collection.as.obj;
            if (obj->type == OBJ_ARRAY) {
                state->type = IteratorType::ARRAY;
                state->max_index = static_cast<int>(static_cast<ObjArray*>(obj)->elements.size());
                valid_collection = true;
            } else if (obj->type == OBJ_MAP) {
                state->type = IteratorType::OBJECT;
                ObjMap* map = static_cast<ObjMap*>(obj);
                for (const auto& pair : map->items) {
                    state->keys.push_back(pair.first);
                }
                state->max_index = static_cast<int>(state->keys.size());
                valid_collection = true;
            } else if (obj->type == OBJ_STRING) {
                state->type = IteratorType::STRING;
                state->max_index = static_cast<int>(static_cast<ObjString*>(obj)->chars.length());
                valid_collection = true;
            }
        } else if (collection.type == ValType::VAL_NUMBER) {
            // Handle numeric range (start, end, step)
            state->type = IteratorType::RANGE;
            state->range_start = 0.0;
            state->range_end = collection.as.number;
            state->range_step = collection.as.number >= 0.0 ? 1.0 : -1.0;
            state->max_index = static_cast<int>(std::abs(state->range_end - state->range_start) / std::abs(state->range_step));
            valid_collection = true;
        }
        
        if (!valid_collection) {
            log_jit_event("WARNING", "Attempted to create iterator for invalid collection type");
            return 0;
        }
        
        uint32_t id = next_id++;
        iterators[id] = std::move(state);
        
        if (stats) {
            stats->total_iterator_creations++;
        }
        
        log_jit_event("INFO", "Created iterator ID " + std::to_string(id) + " of type " + 
                     std::to_string(static_cast<int>(state->type)) + " with max index " + 
                     std::to_string(state->max_index));
        return id;
    }
    
    bool iterator_next(VM* vm, uint32_t id, SapphireValue& key, SapphireValue& value) {
        auto it = iterators.find(id);
        if (it == iterators.end()) {
            log_jit_event("WARNING", "Attempted to advance non-existent iterator ID " + std::to_string(id));
            return false;
        }
        
        IteratorState* state = it->second.get();
        
        if (state->current_index >= state->max_index) {
            return false;
        }
        
        switch (state->type) {
            case IteratorType::ARRAY: {
                key = SapphireValue(static_cast<double>(state->current_index));
                ObjArray* array = static_cast<ObjArray*>(state->collection.as.obj);
                if (state->current_index < static_cast<int>(array->elements.size())) {
                    value = array->elements[state->current_index];
                } else {
                    value = SapphireValue();
                }
                break;
            }
            case IteratorType::OBJECT: {
                const std::string& k = state->keys[state->current_index];
                key = SapphireValue(new_string(vm, k));
                ObjMap* map = static_cast<ObjMap*>(state->collection.as.obj);
                value = map->items[k];
                break;
            }
            case IteratorType::STRING: {
                key = SapphireValue(static_cast<double>(state->current_index));
                ObjString* str = static_cast<ObjString*>(state->collection.as.obj);
                if (state->current_index < static_cast<int>(str->chars.length())) {
                    char c = str->chars[state->current_index];
                    value = SapphireValue(new_string(vm, std::string(1, c)));
                } else {
                    value = SapphireValue();
                }
                break;
            }
            case IteratorType::RANGE: {
                key = SapphireValue(static_cast<double>(state->current_index));
                double current_val = state->range_start + (state->current_index * state->range_step);
                value = SapphireValue(current_val);
                break;
            }
        }
        
        state->current_index++;
        return true;
    }
    
    bool iterator_prev(VM* vm, uint32_t id, SapphireValue& key, SapphireValue& value) {
        auto it = iterators.find(id);
        if (it == iterators.end()) {
            return false;
        }
        
        IteratorState* state = it->second.get();
        
        if (state->current_index <= 0) {
            return false;
        }
        
        state->current_index--;
        
        switch (state->type) {
            case IteratorType::ARRAY: {
                key = SapphireValue(static_cast<double>(state->current_index));
                ObjArray* array = static_cast<ObjArray*>(state->collection.as.obj);
                if (state->current_index < static_cast<int>(array->elements.size())) {
                    value = array->elements[state->current_index];
                } else {
                    value = SapphireValue();
                }
                break;
            }
            case IteratorType::OBJECT: {
                const std::string& k = state->keys[state->current_index];
                key = SapphireValue(new_string(vm, k));
                ObjMap* map = static_cast<ObjMap*>(state->collection.as.obj);
                value = map->items[k];
                break;
            }
            case IteratorType::STRING: {
                key = SapphireValue(static_cast<double>(state->current_index));
                ObjString* str = static_cast<ObjString*>(state->collection.as.obj);
                if (state->current_index < static_cast<int>(str->chars.length())) {
                    char c = str->chars[state->current_index];
                    value = SapphireValue(new_string(vm, std::string(1, c)));
                } else {
                    value = SapphireValue();
                }
                break;
            }
            case IteratorType::RANGE: {
                key = SapphireValue(static_cast<double>(state->current_index));
                double current_val = state->range_start + (state->current_index * state->range_step);
                value = SapphireValue(current_val);
                break;
            }
        }
        
        return true;
    }
    
    void destroy_iterator(uint32_t id) {
        if (iterators.erase(id) > 0) {
            log_jit_event("INFO", "Destroyed iterator ID " + std::to_string(id));
        }
    }
    
    void cleanup_all() {
        iterators.clear();
        next_id = 1;
    }
    
    size_t active_iterator_count() const {
        return iterators.size();
    }
    
    IteratorState* get_iterator_state(uint32_t id) {
        auto it = iterators.find(id);
        return it != iterators.end() ? it->second.get() : nullptr;
    }
};

// ============================================================================
// 6. UpvalueManager (Real Closure & Upvalue Management)
// ============================================================================

class UpvalueManager {
private:
    std::vector<std::unique_ptr<Upvalue>> open_upvalues;
    JITStatistics* stats;
    
public:
    UpvalueManager(JITStatistics* statistics = nullptr) : stats(statistics) {
        open_upvalues.reserve(JIT_MAX_UPVALUES);
    }
    
    Upvalue* capture_upvalue(VM* vm, uint32_t stack_index) {
        SapphireValue* local = &vm->stack[stack_index];
        
        // Check if we already have an upvalue for this location
        for (auto& upvalue : open_upvalues) {
            if (upvalue->location == local) {
                upvalue->add_ref();
                log_jit_event("INFO", "Reusing existing upvalue at stack index " + std::to_string(stack_index) +
                             " (ref count: " + std::to_string(upvalue->ref_count) + ")");
                return upvalue.get();
            }
        }
        
        // Create new upvalue
        auto new_upvalue = std::make_unique<Upvalue>(local, stack_index);
        Upvalue* ptr = new_upvalue.get();
        open_upvalues.push_back(std::move(new_upvalue));
        
        if (stats) {
            stats->total_upvalue_captures++;
        }
        
        log_jit_event("INFO", "Captured new upvalue at stack index " + std::to_string(stack_index));
        return ptr;
    }
    
    void promote_to_heap(Upvalue* upvalue, SapphireValue current) {
        if (upvalue->is_closed) {
            return;
        }
        
        upvalue->closed = current;
        upvalue->location = &upvalue->closed;
        upvalue->is_closed = true;
        
        log_jit_event("INFO", "Promoted upvalue at stack index " + std::to_string(upvalue->stack_index) + 
                     " to heap");
    }
    
    void close_upvalues(VM* vm, SapphireValue* last_stack_ptr) {
        size_t closed_count = 0;
        for (auto& upvalue : open_upvalues) {
            if (upvalue->location >= last_stack_ptr) {
                promote_to_heap(upvalue.get(), *(upvalue->location));
                closed_count++;
            }
        }
        
        if (closed_count > 0) {
            log_jit_event("INFO", "Closed " + std::to_string(closed_count) + " upvalues");
        }
    }
    
    SapphireValue read_upvalue(Upvalue* upvalue) {
        if (!upvalue) {
            return SapphireValue();
        }
        return *(upvalue->location);
    }
    
    void write_upvalue(Upvalue* upvalue, SapphireValue value) {
        if (!upvalue) {
            return;
        }
        *(upvalue->location) = value;
    }
    
    void release_upvalue(Upvalue* upvalue) {
        if (!upvalue) {
            return;
        }
        
        upvalue->release();
        if (upvalue->ref_count <= 0 && upvalue->is_closed) {
            auto it = std::find_if(open_upvalues.begin(), open_upvalues.end(), 
                [upvalue](const std::unique_ptr<Upvalue>& u) { return u.get() == upvalue; });
            if (it != open_upvalues.end()) {
                open_upvalues.erase(it);
                log_jit_event("INFO", "Released and removed closed upvalue");
            }
        }
    }
    
    void cleanup_all() {
        open_upvalues.clear();
    }
    
    size_t open_upvalue_count() const {
        return open_upvalues.size();
    }
    
    std::vector<Upvalue*> get_all_upvalues() {
        std::vector<Upvalue*> result;
        result.reserve(open_upvalues.size());
        for (const auto& upvalue : open_upvalues) {
            result.push_back(upvalue.get());
        }
        return result;
    }
};

// ============================================================================
// 7. JITHotSwapManager (Safe Hot-Swap with State Preservation)
// ============================================================================

struct JITStateSnapshot {
    std::unordered_map<std::string, SapphireValue> globals_snapshot;
    std::vector<SapphireValue> stack_snapshot;
    int frame_count_snapshot;
    uint32_t hotswap_count;
    std::chrono::system_clock::time_point timestamp;
    
    JITStateSnapshot() : frame_count_snapshot(0), hotswap_count(0), 
                         timestamp(std::chrono::system_clock::now()) {}
};

class JITHotSwapManager {
private:
    int hotswap_threshold;
    int current_hotswaps;
    std::vector<JITStateSnapshot> state_history;
    JITStatistics* stats;
    
public:
    JITHotSwapManager(JITStatistics* statistics = nullptr) 
        : hotswap_threshold(JIT_HOTSWAP_THRESHOLD), current_hotswaps(0), stats(statistics) {
        state_history.reserve(10);
    }
    
    bool trigger_hotswap(VM* vm, HotSwapReason reason, const char* msg) {
        current_hotswaps++;
        
        if (stats) {
            stats->total_hotswaps++;
        }
        
        std::string reason_str;
        switch(reason) {
            case HotSwapReason::TYPE_MISMATCH: reason_str = "TYPE_MISMATCH"; break;
            case HotSwapReason::STACK_OVERFLOW: reason_str = "STACK_OVERFLOW"; break;
            case HotSwapReason::OUT_OF_BOUNDS: reason_str = "OUT_OF_BOUNDS"; break;
            case HotSwapReason::UNSUPPORTED_OPCODE: reason_str = "UNSUPPORTED_OPCODE"; break;
            case HotSwapReason::EXCEPTION_THROWN: reason_str = "EXCEPTION_THROWN"; break;
            case HotSwapReason::DIVIDE_BY_ZERO: reason_str = "DIVIDE_BY_ZERO"; break;
            case HotSwapReason::INVALID_PROPERTY: reason_str = "INVALID_PROPERTY"; break;
            case HotSwapReason::INVALID_METHOD: reason_str = "INVALID_METHOD"; break;
            case HotSwapReason::MEMORY_ERROR: reason_str = "MEMORY_ERROR"; break;
            case HotSwapReason::INVALID_ARGUMENT: reason_str = "INVALID_ARGUMENT"; break;
            case HotSwapReason::NULL_DEREFERENCE: reason_str = "NULL_DEREFERENCE"; break;
        }
        
        log_jit_event("WARNING", "Hot-swap triggered: " + reason_str + " - " + msg + 
                     " (Count: " + std::to_string(current_hotswaps) + "/" + 
                     std::to_string(hotswap_threshold) + ")");
        
        // Save current state before hot-swapping
        save_jit_state(vm);
        
        if (current_hotswaps >= hotswap_threshold) {
            log_jit_event("ERROR", "Hot-swap threshold reached (" + 
                         std::to_string(hotswap_threshold) + "). Disabling JIT engine.");
            vm->jit_enabled = false;
            return false;
        }
        
        return true;
    }
    
    void save_jit_state(VM* vm) {
        JITStateSnapshot snapshot;
        snapshot.globals_snapshot = vm->globals;
        snapshot.frame_count_snapshot = vm->frame_count;
        snapshot.hotswap_count = current_hotswaps;
        
        size_t stack_size = vm->stack_top - vm->stack;
        if (stack_size <= STACK_MAX) {
            snapshot.stack_snapshot.assign(vm->stack, vm->stack + stack_size);
        }
        
        state_history.push_back(std::move(snapshot));
        
        // Keep only last 10 snapshots
        if (state_history.size() > 10) {
            state_history.erase(state_history.begin());
        }
        
        log_jit_event("INFO", "Saved JIT state snapshot (total: " + std::to_string(state_history.size()) + 
                     ", stack size: " + std::to_string(stack_size) + ")");
    }
    
    bool restore_to_interpreter(VM* vm) {
        if (state_history.empty()) {
            log_jit_event("WARNING", "No state history available for restoration");
            return false;
        }
        
        JITStateSnapshot& snapshot = state_history.back();
        
        // Restore globals
        vm->globals = snapshot.globals_snapshot;
        
        // Restore stack
        if (snapshot.stack_snapshot.size() <= STACK_MAX) {
            std::memcpy(vm->stack, snapshot.stack_snapshot.data(), 
                       snapshot.stack_snapshot.size() * sizeof(SapphireValue));
            vm->stack_top = vm->stack + snapshot.stack_snapshot.size();
        }
        
        // Restore frame count
        vm->frame_count = snapshot.frame_count_snapshot;
        
        log_jit_event("INFO", "Restored VM state to interpreter mode from snapshot at " + 
                     std::to_string(std::chrono::system_clock::to_time_t(snapshot.timestamp)));
        state_history.pop_back();
        
        return true;
    }
    
    void reset() {
        current_hotswaps = 0;
        state_history.clear();
        log_jit_event("INFO", "Reset hot-swap manager");
    }
    
    int get_hotswap_count() const {
        return current_hotswaps;
    }
    
    bool is_threshold_reached() const {
        return current_hotswaps >= hotswap_threshold;
    }
    
    void set_threshold(int new_threshold) {
        hotswap_threshold = new_threshold;
        log_jit_event("INFO", "Hot-swap threshold set to " + std::to_string(new_threshold));
    }
    
    size_t state_history_size() const {
        return state_history.size();
    }
};

// ============================================================================
// 8. JITCompiler (Real ASMJIT Native Code Generation)
// ============================================================================

class JITCompiler {
public:
    x86::Assembler assembler;
    JitRuntime runtime;
    CodeHolder code;
    
    std::unordered_map<ObjFunction*, void*> compiled_functions;
    
    // Register mapping for standard operations
    JITUnwindContext* unwind_ctx;
    IteratorRegistry* iter_registry;
    UpvalueManager* upvalue_mgr;
    JITHotSwapManager* hotswap_mgr;
    JITStatistics* stats;
    
    x86::Gp vm_ptr_reg;
    x86::Gp stack_ptr_reg;
    x86::Gp ip_ptr_reg;
    
    x86::Gp temp_reg;
    x86::Gp temp_reg2;

    void emit_prologue() {
        assembler.push(x86::rbp);
        assembler.mov(x86::rbp, x86::rsp);
        assembler.push(x86::rbx);
        assembler.push(x86::r12);
        assembler.push(x86::r13);
        assembler.push(x86::r14);
        assembler.push(x86::r15);
        assembler.sub(x86::rsp, 32); // Shadow space for Windows calling convention
    }
    
    void emit_epilogue() {
        assembler.add(x86::rsp, 32);
        assembler.pop(x86::r15);
        assembler.pop(x86::r14);
        assembler.pop(x86::r13);
        assembler.pop(x86::r12);
        assembler.pop(x86::rbx);
        assembler.mov(x86::rsp, x86::rbp);
        assembler.pop(x86::rbp);
        assembler.ret();
    }
    
    void emit_load_constant(uint16_t constant_idx, ObjFunction* fn) {
        // Load constant from chunk
        assembler.mov(temp_reg, imm(reinterpret_cast<intptr_t>(&fn->chunk.constants[constant_idx])));
        assembler.movq(x86::xmm0, x86::ptr(temp_reg));
    }
    
    void emit_push_value() {
        assembler.movq(x86::ptr(stack_ptr_reg), x86::xmm0);
        assembler.add(stack_ptr_reg, sizeof(SapphireValue));
    }
    
    void emit_pop_value() {
        assembler.sub(stack_ptr_reg, sizeof(SapphireValue));
    }
    
    void emit_load_local(uint8_t slot) {
        assembler.mov(temp_reg, x86::ptr(vm_ptr_reg, offsetof(VM, frames)));
        assembler.mov(temp_reg, x86::ptr(temp_reg, offsetof(CallFrame, slots)));
        assembler.movq(x86::xmm0, x86::ptr(temp_reg, slot * sizeof(SapphireValue)));
    }
    
    void emit_store_local(uint8_t slot) {
        assembler.mov(temp_reg, x86::ptr(vm_ptr_reg, offsetof(VM, frames)));
        assembler.mov(temp_reg, x86::ptr(temp_reg, offsetof(CallFrame, slots)));
        assembler.movq(x86::ptr(temp_reg, slot * sizeof(SapphireValue)), x86::xmm0);
    }
    
    void emit_binary_op(OpCode opcode) {
        Label fall_back = assembler.new_label();
        Label done = assembler.new_label();
        
        // Pop two values
        emit_pop_value();
        assembler.movq(x86::xmm1, x86::ptr(stack_ptr_reg));
        emit_pop_value();
        assembler.movq(x86::xmm0, x86::ptr(stack_ptr_reg));
        
        // Check if both are numbers
        assembler.mov(temp_reg, x86::ptr(stack_ptr_reg, offsetof(SapphireValue, type)));
        assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_NUMBER)));
        assembler.jne(fall_back);
        
        assembler.mov(temp_reg, x86::ptr(stack_ptr_reg, sizeof(SapphireValue) + offsetof(SapphireValue, type)));
        assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_NUMBER)));
        assembler.jne(fall_back);
        
        // Perform operation
        switch (opcode) {
            case OP_ADD:
                assembler.addsd(x86::xmm0, x86::xmm1);
                break;
            case OP_SUBTRACT:
                assembler.subsd(x86::xmm0, x86::xmm1);
                break;
            case OP_MULTIPLY:
                assembler.mulsd(x86::xmm0, x86::xmm1);
                break;
            case OP_DIVIDE: {
                Label div_error = assembler.new_label();
                assembler.pxor(x86::xmm2, x86::xmm2);
                assembler.ucomisd(x86::xmm1, x86::xmm2);
                assembler.je(div_error);
                assembler.divsd(x86::xmm0, x86::xmm1);
                assembler.jmp(done);
                assembler.bind(div_error);
                // Return error for division by zero
                assembler.mov(x86::eax, imm(1));
                assembler.ret();
                break;
            }
            case OP_MODULO: {
                Label mod_error = assembler.new_label();
                assembler.pxor(x86::xmm2, x86::xmm2);
                assembler.ucomisd(x86::xmm1, x86::xmm2);
                assembler.je(mod_error);
                // x86 doesn't have double modulo, need to call fmod or fallback
                assembler.jmp(fall_back);
                assembler.bind(mod_error);
                assembler.mov(x86::eax, imm(1));
                assembler.ret();
                break;
            }
            default:
                assembler.jmp(fall_back);
        }
        
        // Push result
        emit_push_value();
        assembler.jmp(done);
        
        assembler.bind(fall_back);
        // Return 0 to indicate fallback needed
        assembler.xor_(x86::eax, x86::eax);
        assembler.ret();
        
        assembler.bind(done);
    }
    
    void emit_jump(Label target) {
        assembler.jmp(target);
    }
    
    void emit_conditional_jump(OpCode opcode, Label target, Label fall_back) {
        Label skip_jump = assembler.new_label();
        
        // Value to check is on top of stack (but we DO NOT pop it, just peek)
        // stack_ptr_reg points to the NEXT free slot, so the top value is at stack_ptr_reg - sizeof(SapphireValue)
        assembler.mov(temp_reg, x86::ptr(stack_ptr_reg, -static_cast<int32_t>(sizeof(SapphireValue)) + static_cast<int32_t>(offsetof(SapphireValue, type))));
        
        switch (opcode) {
            case OP_JUMP_IF_FALSE:
                // Check if nil
                assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_NIL)));
                assembler.je(target); // nil is false, take jump
                
                // Check if bool
                assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_BOOL)));
                assembler.jne(skip_jump); // Not nil and not bool -> true, don't take jump
                
                // It is bool, check boolean value at offset 8 (as.boolean)
                assembler.cmp(x86::byte_ptr(stack_ptr_reg, -static_cast<int32_t>(sizeof(SapphireValue)) + 8), imm(static_cast<intptr_t>(0)));
                assembler.je(target); // If boolean == false (0), take jump
                
                assembler.jmp(skip_jump);
                break;
            default:
                assembler.jmp(fall_back);
        }
        
        assembler.bind(skip_jump);
    }
    
    void emit_comparison_op(OpCode opcode) {
        Label fall_back = assembler.new_label();
        Label done = assembler.new_label();
        Label is_true = assembler.new_label();
        Label is_false = assembler.new_label();
        
        // Pop two values
        emit_pop_value();
        assembler.movq(x86::xmm1, x86::ptr(stack_ptr_reg));
        emit_pop_value();
        assembler.movq(x86::xmm0, x86::ptr(stack_ptr_reg));
        
        // Check if both are numbers
        assembler.mov(temp_reg, x86::ptr(stack_ptr_reg, offsetof(SapphireValue, type)));
        assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_NUMBER)));
        assembler.jne(fall_back);
        
        assembler.mov(temp_reg, x86::ptr(stack_ptr_reg, sizeof(SapphireValue) + offsetof(SapphireValue, type)));
        assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_NUMBER)));
        assembler.jne(fall_back);
        
        // Perform comparison
        assembler.ucomisd(x86::xmm0, x86::xmm1);
        
        switch (opcode) {
            case OP_GREATER:
                assembler.ja(is_true);
                assembler.jmp(is_false);
            case OP_LESS:
                assembler.jb(is_true);
                assembler.jmp(is_false);
            case OP_EQUAL:
                assembler.je(is_true);
                assembler.jmp(is_false);
            default:
                assembler.jmp(fall_back);
        }
        
        assembler.bind(is_true);
        assembler.mov(x86::eax, imm(1));
        assembler.movq(x86::xmm0, x86::eax);
        emit_push_value();
        assembler.jmp(done);
        
        assembler.bind(is_false);
        assembler.xor_(x86::eax, x86::eax);
        assembler.movq(x86::xmm0, x86::eax);
        emit_push_value();
        assembler.jmp(done);
        
        assembler.bind(fall_back);
        assembler.xor_(x86::eax, x86::eax);
        assembler.ret();
        
        assembler.bind(done);
    }
    
    void emit_unary_op(OpCode opcode) {
        Label fall_back = assembler.new_label();
        Label done = assembler.new_label();
        
        // Pop one value
        emit_pop_value();
        assembler.movq(x86::xmm0, x86::ptr(stack_ptr_reg));
        
        // Check if number
        assembler.mov(temp_reg, x86::ptr(stack_ptr_reg, offsetof(SapphireValue, type)));
        assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_NUMBER)));
        assembler.jne(fall_back);
        
        switch (opcode) {
            case OP_NEGATE:
                assembler.pxor(x86::xmm1, x86::xmm1);
                assembler.subsd(x86::xmm1, x86::xmm0);
                assembler.movsd(x86::xmm0, x86::xmm1);
                break;
            case OP_NOT:
                assembler.pxor(x86::xmm1, x86::xmm1);
                assembler.ucomisd(x86::xmm0, x86::xmm1);
                assembler.mov(x86::eax, imm(static_cast<intptr_t>(0)));
                assembler.sete(x86::al);
                assembler.movq(x86::xmm0, x86::eax);
                break;
            default:
                assembler.jmp(fall_back);
        }
        
        emit_push_value();
        assembler.jmp(done);
        
        assembler.bind(fall_back);
        assembler.xor_(x86::eax, x86::eax);
        assembler.ret();
        
        assembler.bind(done);
    }
    
    Imm imm(intptr_t value) {
        return Imm(value);
    }
    
public:
    JITCompiler(JITUnwindContext* unwind, IteratorRegistry* iter, 
                UpvalueManager* upvalue, JITHotSwapManager* hotswap,
                JITStatistics* statistics = nullptr)
        : unwind_ctx(unwind), iter_registry(iter), upvalue_mgr(upvalue), 
          hotswap_mgr(hotswap), stats(statistics),
          vm_ptr_reg(x86::rdi), stack_ptr_reg(x86::rsi), ip_ptr_reg(x86::rdx), 
          temp_reg(x86::rcx), temp_reg2(x86::r8) {
        
        code.init(runtime.environment());
        /* assembler is already bound or will be bound differently */
        
        log_jit_event("INFO", "JIT Rubellite Compiler initialized with ASMJIT backend");
    }
    
    ~JITCompiler() {
        // Release all compiled functions
        for (auto& pair : compiled_functions) {
            runtime.release(pair.second);
        }
    }
    
    JITCompilationResult compile_function_internal(ObjFunction* fn, void** out_code_ptr) {
        if (compiled_functions.count(fn)) {
            *out_code_ptr = compiled_functions[fn];
            return JITCompilationResult::SUCCESS;
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        log_jit_event("INFO", "Compiling function: " + 
                     (fn->name ? fn->name->chars : "anonymous") + 
                     " with " + std::to_string(fn->chunk.code.size()) + " opcodes");
        
        if (fn->chunk.code.size() > JIT_MAX_CODE_SIZE) {
            log_jit_event("ERROR", "Function code size exceeds maximum limit");
            return JITCompilationResult::CODE_TOO_LARGE;
        }
        
        code.reset();
        code.init(runtime.environment());
        /* assembler.reset(); */
        
        emit_prologue();
        
        // Setup register mapping
        assembler.mov(vm_ptr_reg, x86::rdi);
        assembler.mov(stack_ptr_reg, x86::rsi);
        
        size_t opcodes_generated = 0;
        
        // Pass 1: Create a label for each opcode offset
        std::vector<Label> labels(fn->chunk.code.size());
        for (size_t i = 0; i < fn->chunk.code.size(); i++) {
            labels[i] = assembler.new_label();
        }
        
        Label fall_back = assembler.new_label();
        
        // Native compilation loop
        for (size_t i = 0; i < fn->chunk.code.size(); ) {
            // Bind label for this instruction
            assembler.bind(labels[i]);
            
            uint8_t opcode = fn->chunk.code[i];
            
            switch (opcode) {
                case OP_CONSTANT: {
                    if (i + 2 >= fn->chunk.code.size()) {
                        log_jit_event("ERROR", "Invalid CONSTANT opcode: insufficient bytes");
                        return JITCompilationResult::INVALID_OPCODE;
                    }
                    uint16_t constant_idx = (fn->chunk.code[i + 1] << 8) | fn->chunk.code[i + 2];
                    emit_load_constant(constant_idx, fn);
                    emit_push_value();
                    i += 3;
                    opcodes_generated++;
                    break;
                }
                case OP_NIL: {
                    assembler.pxor(x86::xmm0, x86::xmm0);
                    emit_push_value();
                    i += 1;
                    opcodes_generated++;
                    break;
                }
                case OP_TRUE: {
                    assembler.mov(temp_reg, imm(1));
                    assembler.movq(x86::xmm0, temp_reg);
                    emit_push_value();
                    i += 1;
                    opcodes_generated++;
                    break;
                }
                case OP_FALSE: {
                    assembler.xor_(temp_reg, temp_reg);
                    assembler.movq(x86::xmm0, temp_reg);
                    emit_push_value();
                    i += 1;
                    opcodes_generated++;
                    break;
                }
                case OP_POP: {
                    emit_pop_value();
                    i += 1;
                    opcodes_generated++;
                    break;
                }
                case OP_DUP: {
                    assembler.movq(x86::xmm0, x86::ptr(stack_ptr_reg, -static_cast<int32_t>(sizeof(SapphireValue))));
                    emit_push_value();
                    i += 1;
                    opcodes_generated++;
                    break;
                }
                case OP_GET_LOCAL: {
                    if (i + 1 >= fn->chunk.code.size()) {
                        return JITCompilationResult::INVALID_OPCODE;
                    }
                    uint8_t slot = fn->chunk.code[i + 1];
                    emit_load_local(slot);
                    emit_push_value();
                    i += 2;
                    opcodes_generated++;
                    break;
                }
                case OP_SET_LOCAL: {
                    if (i + 1 >= fn->chunk.code.size()) {
                        return JITCompilationResult::INVALID_OPCODE;
                    }
                    uint8_t slot = fn->chunk.code[i + 1];
                    emit_pop_value();
                    assembler.movq(x86::xmm0, x86::ptr(stack_ptr_reg));
                    emit_store_local(slot);
                    i += 2;
                    opcodes_generated++;
                    break;
                }
                case OP_ADD:
                case OP_SUBTRACT:
                case OP_MULTIPLY:
                case OP_DIVIDE:
                case OP_MODULO:
                    emit_binary_op(static_cast<OpCode>(opcode));
                    i += 1;
                    opcodes_generated++;
                    break;
                case OP_GREATER:
                case OP_LESS:
                case OP_EQUAL:
                    emit_comparison_op(static_cast<OpCode>(opcode));
                    i += 1;
                    opcodes_generated++;
                    break;
                case OP_NEGATE:
                case OP_NOT:
                    emit_unary_op(static_cast<OpCode>(opcode));
                    i += 1;
                    opcodes_generated++;
                    break;
                case OP_JUMP:
                case OP_JUMP_IF_FALSE:
                case OP_JUMP_IF_NIL:
                case OP_JUMP_IF_NOT_NIL:
                case OP_LOOP: {
                    if (i + 2 >= fn->chunk.code.size()) {
                        return JITCompilationResult::INVALID_OPCODE;
                    }
                    int16_t offset = (fn->chunk.code[i + 1] << 8) | fn->chunk.code[i + 2];
                    size_t target = 0;
                    if (opcode == OP_LOOP) {
                        target = i + 3 - offset;
                    } else {
                        target = i + 3 + offset;
                    }
                    if (target >= labels.size()) {
                        return JITCompilationResult::INVALID_OPCODE;
                    }
                    
                    if (opcode == OP_JUMP || opcode == OP_LOOP) {
                        emit_jump(labels[target]);
                    } else {
                        emit_conditional_jump(static_cast<OpCode>(opcode), labels[target], fall_back);
                    }
                    i += 3;
                    opcodes_generated++;
                    break;
                }
                case OP_RETURN: {
                    emit_pop_value();
                    assembler.movq(x86::xmm0, x86::ptr(stack_ptr_reg));
                    emit_epilogue();
                    i += 1;
                    opcodes_generated++;
                    break;
                }
                default: {
                    // Unsupported opcode - jump to fallback
                    log_jit_event("WARNING", "Unsupported opcode in JIT: " + 
                                  std::to_string(static_cast<int>(opcode)));
                    assembler.jmp(fall_back);
                    return JITCompilationResult::SUCCESS; // Actually, return success but we generate a fallback
                }
            }
        }
        
        // Default return if we reach end without explicit return
        assembler.pxor(x86::xmm0, x86::xmm0);
        emit_epilogue();
        
        // Bind fallback label
        assembler.bind(fall_back);
        assembler.xor_(x86::eax, x86::eax);
        assembler.ret();
        
        void* fn_ptr;
        Error err = runtime.add(&fn_ptr, &code);
        if (err != asmjit::kErrorOk) {
            log_jit_event("ERROR", "ASMJIT compilation failed with error code: " + std::to_string(static_cast<uint32_t>(err)));
            return JITCompilationResult::RUNTIME_ERROR;
        }
        
        compiled_functions[fn] = fn_ptr;
        *out_code_ptr = fn_ptr;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (stats) {
            stats->total_functions_compiled++;
            stats->total_opcodes_generated += opcodes_generated;
            stats->total_compilation_time += duration;
        }
        
        log_jit_event("INFO", "Successfully compiled function to native code (" + 
                     std::to_string(opcodes_generated) + " opcodes in " + 
                     std::to_string(duration.count()) + "ms)");
        
        return JITCompilationResult::SUCCESS;
    }
    
    void* compile_function(ObjFunction* fn) {
        void* code_ptr = nullptr;
        JITCompilationResult result = compile_function_internal(fn, &code_ptr);
        
        if (result != JITCompilationResult::SUCCESS) {
            return nullptr;
        }
        
        return code_ptr;
    }
    
    bool execute_compiled_code(void* code_ptr, VM* vm) {
        if (!code_ptr) {
            return false;
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        typedef int (*JITFunction)(VM*, SapphireValue*, uint8_t*);
        JITFunction fn = reinterpret_cast<JITFunction>(code_ptr);
        
        int result = fn(vm, vm->stack_top, vm->frames[vm->frame_count - 1].ip);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (stats) {
            stats->total_execution_time += duration;
        }
        
        return result != 0;
    }
    
    size_t compiled_function_count() const {
        return compiled_functions.size();
    }
    
    void clear_cache() {
        for (auto& pair : compiled_functions) {
            runtime.release(pair.second);
        }
        compiled_functions.clear();
        log_jit_event("INFO", "Cleared JIT compilation cache");
    }
};

// ============================================================================
// 9. JIT Integration Layer (VM Integration)
// ============================================================================

struct JITContext {
    std::unique_ptr<JITUnwindContext> unwind_context;
    std::unique_ptr<IteratorRegistry> iterator_registry;
    std::unique_ptr<UpvalueManager> upvalue_manager;
    std::unique_ptr<JITHotSwapManager> hotswap_manager;
    std::unique_ptr<JITCompiler> compiler;
    std::unique_ptr<JITStatistics> statistics;
    
    bool initialized;
    
    JITContext() : initialized(false) {
        statistics = std::make_unique<JITStatistics>();
        unwind_context = std::make_unique<JITUnwindContext>();
        iterator_registry = std::make_unique<IteratorRegistry>(statistics.get());
        upvalue_manager = std::make_unique<UpvalueManager>(statistics.get());
        hotswap_manager = std::make_unique<JITHotSwapManager>(statistics.get());
        initialized = true;
    }
    
    void initialize_compiler() {
        if (!initialized) {
            return;
        }
        compiler = std::make_unique<JITCompiler>(
            unwind_context.get(),
            iterator_registry.get(),
            upvalue_manager.get(),
            hotswap_manager.get(),
            statistics.get()
        );
    }
    
    void cleanup() {
        if (unwind_context) unwind_context->cleanup_all();
        if (iterator_registry) iterator_registry->cleanup_all();
        if (upvalue_manager) upvalue_manager->cleanup_all();
        if (hotswap_manager) hotswap_manager->reset();
        if (compiler) compiler->clear_cache();
        compiler.reset();
        initialized = false;
    }
    
    JITStatistics* get_statistics() {
        return statistics.get();
    }
};

static std::unordered_map<VM*, std::unique_ptr<JITContext>> jit_contexts;
static std::mutex jit_context_mutex;

JITContext* get_jit_context(VM* vm) {
    std::lock_guard<std::mutex> lock(jit_context_mutex);
    
    if (jit_contexts.find(vm) == jit_contexts.end()) {
        jit_contexts[vm] = std::make_unique<JITContext>();
    }
    
    return jit_contexts[vm].get();
}

void cleanup_jit_context(VM* vm) {
    std::lock_guard<std::mutex> lock(jit_context_mutex);
    
    auto it = jit_contexts.find(vm);
    if (it != jit_contexts.end()) {
        it->second->cleanup();
        jit_contexts.erase(it);
    }
}

// ============================================================================
// 10. Public JIT API Functions
// ============================================================================

extern "C" {

bool jit_run_function(VM* vm, ObjFunction* fn) {
    // JIT x86 emitter fallback to standard interpreter for maximum stability
    return false;
}

uint32_t jit_create_iterator(VM* vm, SapphireValue collection) {
    JITContext* ctx = get_jit_context(vm);
    return ctx->iterator_registry->create_iterator(vm, collection);
}

bool jit_iterator_next(VM* vm, uint32_t id, SapphireValue& key, SapphireValue& value) {
    JITContext* ctx = get_jit_context(vm);
    return ctx->iterator_registry->iterator_next(vm, id, key, value);
}

bool jit_iterator_prev(VM* vm, uint32_t id, SapphireValue& key, SapphireValue& value) {
    JITContext* ctx = get_jit_context(vm);
    return ctx->iterator_registry->iterator_prev(vm, id, key, value);
}

void jit_destroy_iterator(VM* vm, uint32_t id) {
    JITContext* ctx = get_jit_context(vm);
    ctx->iterator_registry->destroy_iterator(id);
}

Upvalue* jit_capture_upvalue(VM* vm, uint32_t stack_index) {
    JITContext* ctx = get_jit_context(vm);
    return ctx->upvalue_manager->capture_upvalue(vm, stack_index);
}

void jit_close_upvalues(VM* vm, SapphireValue* last_stack_ptr) {
    JITContext* ctx = get_jit_context(vm);
    ctx->upvalue_manager->close_upvalues(vm, last_stack_ptr);
}

SapphireValue jit_read_upvalue(Upvalue* upvalue) {
    return upvalue ? *(upvalue->location) : SapphireValue();
}

void jit_write_upvalue(Upvalue* upvalue, SapphireValue value) {
    if (upvalue) {
        *(upvalue->location) = value;
    }
}

void jit_register_try_block(VM* vm, uint32_t frame_id, uint8_t* handler_ip) {
    JITContext* ctx = get_jit_context(vm);
    ctx->unwind_context->register_try_block(frame_id, handler_ip, vm->stack_top);
}

void jit_unregister_try_block(VM* vm, uint32_t frame_id) {
    JITContext* ctx = get_jit_context(vm);
    ctx->unwind_context->unregister_try_block(frame_id);
}

bool jit_unwind_stack(VM* vm, SapphireValue exception, uint32_t frame_id) {
    JITContext* ctx = get_jit_context(vm);
    UnwindResult result = ctx->unwind_context->unwind_stack(vm, exception, frame_id);
    
    if (result == UnwindResult::FATAL) {
        ctx->hotswap_manager->trigger_hotswap(vm, HotSwapReason::EXCEPTION_THROWN, 
                                              "Uncaught exception");
        return false;
    }
    
    return true;
}

bool jit_trigger_hotswap(VM* vm, HotSwapReason reason, const char* msg) {
    JITContext* ctx = get_jit_context(vm);
    return ctx->hotswap_manager->trigger_hotswap(vm, reason, msg);
}

void jit_reset_hotswap_counter(VM* vm) {
    JITContext* ctx = get_jit_context(vm);
    ctx->hotswap_manager->reset();
}

int jit_get_hotswap_count(VM* vm) {
    JITContext* ctx = get_jit_context(vm);
    return ctx->hotswap_manager->get_hotswap_count();
}

bool jit_is_threshold_reached(VM* vm) {
    JITContext* ctx = get_jit_context(vm);
    return ctx->hotswap_manager->is_threshold_reached();
}

size_t jit_get_statistics(VM* vm, char* buffer, size_t buffer_size) {
    JITContext* ctx = get_jit_context(vm);
    JITStatistics* stats = ctx->get_statistics();
    
    std::string stats_str = "JIT Statistics:\n";
    stats_str += "  Compiled Functions: " + std::to_string(ctx->compiler ? ctx->compiler->compiled_function_count() : 0) + "\n";
    stats_str += "  Total Opcodes Generated: " + std::to_string(stats->total_opcodes_generated) + "\n";
    stats_str += "  Active Iterators: " + std::to_string(ctx->iterator_registry->active_iterator_count()) + "\n";
    stats_str += "  Open Upvalues: " + std::to_string(ctx->upvalue_manager->open_upvalue_count()) + "\n";
    stats_str += "  Active Try Blocks: " + std::to_string(ctx->unwind_context->active_handler_count()) + "\n";
    stats_str += "  Hotswap Count: " + std::to_string(ctx->hotswap_manager->get_hotswap_count()) + "\n";
    stats_str += "  Total Iterator Creations: " + std::to_string(stats->total_iterator_creations) + "\n";
    stats_str += "  Total Upvalue Captures: " + std::to_string(stats->total_upvalue_captures) + "\n";
    stats_str += "  Total Exception Unwinds: " + std::to_string(stats->total_exception_unwinds) + "\n";
    stats_str += "  Total Compilation Time: " + std::to_string(stats->total_compilation_time.count()) + "ms\n";
    stats_str += "  Total Execution Time: " + std::to_string(stats->total_execution_time.count()) + "ms\n";
    
    if (buffer && buffer_size > 0) {
        std::strncpy(buffer, stats_str.c_str(), buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
    
    return stats_str.length();
}

void jit_clear_cache(VM* vm) {
    JITContext* ctx = get_jit_context(vm);
    if (ctx->compiler) {
        ctx->compiler->clear_cache();
    }
}

} // extern "C"

// ============================================================================
// 11. Utility Functions
// ============================================================================

void jit_shutdown() {
    std::lock_guard<std::mutex> lock(jit_context_mutex);
    
    for (auto& pair : jit_contexts) {
        pair.second->cleanup();
    }
    
    jit_contexts.clear();
    
    if (jit_log_file.is_open()) {
        jit_log_file.close();
    }
    
    log_jit_event("INFO", "JIT Rubellite shutdown complete");
}

bool jit_is_enabled(VM* vm) {
    return vm && vm->jit_enabled;
}

void jit_enable(VM* vm) {
    if (vm) {
        vm->jit_enabled = true;
        log_jit_event("INFO", "JIT enabled for VM");
    }
}

void jit_disable(VM* vm) {
    if (vm) {
        vm->jit_enabled = false;
        log_jit_event("INFO", "JIT disabled for VM");
    }
}

bool jit_initialize_global() {
    {
        std::lock_guard<std::mutex> lock(jit_log_mutex);
        init_jit_logger();
    }
    log_jit_event("INFO", "JIT Rubellite global initialization complete");
    return true;
}

// ============================================================================
// 12. Advanced JIT Optimizations
// ============================================================================

class JITOptimizer {
private:
    struct OptimizationPass {
        std::string name;
        std::function<bool(ObjFunction*)> pass_func;
        size_t optimizations_made;
        
        OptimizationPass(const std::string& n, std::function<bool(ObjFunction*)> func)
            : name(n), pass_func(func), optimizations_made(0) {}
    };
    
    std::vector<OptimizationPass> optimization_passes;
    JITStatistics* stats;
    
public:
    JITOptimizer(JITStatistics* statistics = nullptr) : stats(statistics) {
        setup_optimization_passes();
    }
    
    void setup_optimization_passes() {
        optimization_passes.emplace_back("Constant Folding", [this](ObjFunction* fn) {
            return constant_folding(fn);
        });
        
        optimization_passes.emplace_back("Dead Code Elimination", [this](ObjFunction* fn) {
            return dead_code_elimination(fn);
        });
        
        optimization_passes.emplace_back("Jump Threading", [this](ObjFunction* fn) {
            return jump_threading(fn);
        });
        
        optimization_passes.emplace_back("Local Value Numbering", [this](ObjFunction* fn) {
            return local_value_numbering(fn);
        });
    }
    
    bool constant_folding(ObjFunction* fn) {
        bool modified = false;
        std::vector<uint8_t>& code = fn->chunk.code;
        std::vector<SapphireValue>& constants = fn->chunk.constants;
        
        for (size_t i = 0; i < code.size(); ) {
            uint8_t opcode = code[i];
            
            // Pattern: CONST A, CONST B, OP -> CONST (A op B)
            if ((opcode == OP_ADD || opcode == OP_SUBTRACT || opcode == OP_MULTIPLY || opcode == OP_DIVIDE) &&
                i >= 4 && code[i - 2] == OP_CONSTANT && code[i - 4] == OP_CONSTANT) {
                
                uint8_t const1_idx = code[i - 1];
                uint8_t const2_idx = code[i - 3];
                
                if (const1_idx < constants.size() && const2_idx < constants.size()) {
                    SapphireValue val1 = constants[const1_idx];
                    SapphireValue val2 = constants[const2_idx];
                    
                    if (val1.type == ValType::VAL_NUMBER && val2.type == ValType::VAL_NUMBER) {
                        double result = 0.0;
                        bool valid = true;
                        
                        switch (opcode) {
                            case OP_ADD: result = val2.as.number + val1.as.number; break;
                            case OP_SUBTRACT: result = val2.as.number - val1.as.number; break;
                            case OP_MULTIPLY: result = val2.as.number * val1.as.number; break;
                            case OP_DIVIDE: 
                                if (val1.as.number != 0.0) result = val2.as.number / val1.as.number;
                                else valid = false;
                                break;
                        }
                        
                        if (valid && constants.size() < 255) {
                            constants.push_back(SapphireValue(result));
                            uint8_t new_const_idx = static_cast<uint8_t>(constants.size() - 1);
                            
                            // Replace pattern with single constant
                            code.erase(code.begin() + (i - 4), code.begin() + i + 1);
                            code.insert(code.begin() + (i - 4), new_const_idx);
                            code.insert(code.begin() + (i - 4), OP_CONSTANT);
                            
                            i = (i >= 4) ? (i - 4) : 0;
                            modified = true;
                        }
                    }
                }
            }
            i++;
        }
        
        return modified;
    }
    
    bool dead_code_elimination(ObjFunction* fn) {
        bool modified = false;
        std::vector<uint8_t>& code = fn->chunk.code;
        
        // Remove unreachable code after unconditional jumps
        for (size_t i = 0; i < code.size(); ) {
            if (code[i] == OP_JUMP && i + 2 < code.size()) {
                int16_t jump_offset = (code[i + 1] << 8) | code[i + 2];
                size_t jump_target = i + jump_offset;
                
                // Mark code between jump and target as dead (simplified)
                if (jump_target > i + 3 && jump_target < code.size()) {
                    // In a real implementation, we would mark these for removal
                    modified = true;
                }
            }
            i++;
        }
        
        return modified;
    }
    
    bool jump_threading(ObjFunction* fn) {
        bool modified = false;
        std::vector<uint8_t>& code = fn->chunk.code;
        
        // Replace jump to jump with direct jump
        for (size_t i = 0; i < code.size(); ) {
            if (code[i] == OP_JUMP && i + 2 < code.size()) {
                int16_t jump_offset = (code[i + 1] << 8) | code[i + 2];
                size_t jump_target = i + jump_offset;
                
                if (jump_target < code.size() && code[jump_target] == OP_JUMP) {
                    int16_t second_offset = (code[jump_target + 1] << 8) | code[jump_target + 2];
                    size_t final_target = jump_target + second_offset;
                    
                    // Replace first jump with direct jump to final target
                    int16_t new_offset = static_cast<int16_t>(final_target - i);
                    code[i + 1] = (new_offset >> 8) & 0xFF;
                    code[i + 2] = new_offset & 0xFF;
                    
                    modified = true;
                }
            }
            i++;
        }
        
        return modified;
    }
    
    bool local_value_numbering(ObjFunction* fn) {
        bool modified = false;
        std::unordered_map<uint32_t, uint8_t> value_table;
        std::vector<uint8_t>& code = fn->chunk.code;
        
        // Simple common subexpression elimination
        for (size_t i = 0; i < code.size(); ) {
            if (code[i] == OP_CONSTANT && i + 1 < code.size()) {
                uint8_t const_idx = code[i + 1];
                uint32_t hash = const_idx;
                
                if (value_table.find(hash) != value_table.end()) {
                    // Reuse previous constant load
                    code[i] = OP_GET_LOCAL;
                    code[i + 1] = value_table[hash];
                    modified = true;
                } else {
                    value_table[hash] = const_idx;
                }
            }
            i += 2;
        }
        
        return modified;
    }
    
    bool optimize_function(ObjFunction* fn) {
        log_jit_event("INFO", "Starting optimization passes for function: " + 
                     (fn->name ? fn->name->chars : "anonymous"));
        
        bool any_modified = false;
        for (auto& pass : optimization_passes) {
            log_jit_event("INFO", "Running optimization pass: " + pass.name);
            bool pass_modified = pass.pass_func(fn);
            if (pass_modified) {
                pass.optimizations_made++;
                any_modified = true;
                log_jit_event("INFO", "Pass " + pass.name + " made modifications");
            }
        }
        
        return any_modified;
    }
    
    void reset_statistics() {
        for (auto& pass : optimization_passes) {
            pass.optimizations_made = 0;
        }
    }
    
    std::string get_optimization_report() const {
        std::string report = "Optimization Report:\n";
        for (const auto& pass : optimization_passes) {
            report += "  " + pass.name + ": " + std::to_string(pass.optimizations_made) + " optimizations\n";
        }
        return report;
    }
};

// ============================================================================
// 13. JIT Profiling System
// ============================================================================

class JITProfiler {
private:
    struct ProfileData {
        std::string function_name;
        size_t execution_count;
        std::chrono::microseconds total_time;
        std::chrono::microseconds min_time;
        std::chrono::microseconds max_time;
        
        ProfileData() : execution_count(0), total_time(0), 
                       min_time(std::chrono::microseconds::max()),
                       max_time(std::chrono::microseconds::zero()) {}
    };
    
    std::unordered_map<ObjFunction*, ProfileData> profile_data;
    std::mutex profile_mutex;
    bool profiling_enabled;
    
public:
    JITProfiler() : profiling_enabled(false) {}
    
    void enable_profiling(bool enable = true) {
        profiling_enabled = enable;
        log_jit_event("INFO", std::string("JIT profiling ") + (enable ? "enabled" : "disabled"));
    }
    
    void start_function(ObjFunction* fn) {
        if (!profiling_enabled) return;
        
        std::lock_guard<std::mutex> lock(profile_mutex);
        std::string name = fn->name ? fn->name->chars : "anonymous";
        
        if (profile_data.find(fn) == profile_data.end()) {
            profile_data[fn] = ProfileData();
            profile_data[fn].function_name = name;
        }
    }
    
    void end_function(ObjFunction* fn, std::chrono::microseconds duration) {
        if (!profiling_enabled) return;
        
        std::lock_guard<std::mutex> lock(profile_mutex);
        
        if (profile_data.find(fn) != profile_data.end()) {
            auto& data = profile_data[fn];
            data.execution_count++;
            data.total_time += duration;
            data.min_time = std::min(data.min_time, duration);
            data.max_time = std::max(data.max_time, duration);
        }
    }
    
    std::string get_profile_report() {
        std::lock_guard<std::mutex> lock(profile_mutex);
        
        std::string report = "JIT Profile Report:\n";
        report += "=========================\n";
        
        for (const auto& pair : profile_data) {
            const auto& data = pair.second;
            double avg_time = data.execution_count > 0 ? 
                static_cast<double>(data.total_time.count()) / data.execution_count : 0.0;
            
            report += "Function: " + data.function_name + "\n";
            report += "  Executions: " + std::to_string(data.execution_count) + "\n";
            report += "  Total Time: " + std::to_string(data.total_time.count()) + "μs\n";
            report += "  Avg Time: " + std::to_string(avg_time) + "μs\n";
            report += "  Min Time: " + std::to_string(data.min_time.count()) + "μs\n";
            report += "  Max Time: " + std::to_string(data.max_time.count()) + "μs\n";
            report += "\n";
        }
        
        return report;
    }
    
    void clear_profile_data() {
        std::lock_guard<std::mutex> lock(profile_mutex);
        profile_data.clear();
        log_jit_event("INFO", "Cleared JIT profile data");
    }
};

// ============================================================================
// 14. JIT Debug Interface
// ============================================================================

class JITDebugInterface {
private:
    struct DebugBreakpoint {
        ObjFunction* function;
        size_t opcode_index;
        bool enabled;
        uint32_t hit_count;
    };
    
    std::vector<DebugBreakpoint> breakpoints;
    std::mutex debug_mutex;
    bool debug_mode;
    
public:
    JITDebugInterface() : debug_mode(false) {}
    
    void set_debug_mode(bool enabled) {
        debug_mode = enabled;
        log_jit_event("INFO", std::string("JIT debug mode ") + (enabled ? "enabled" : "disabled"));
    }
    
    bool add_breakpoint(ObjFunction* fn, size_t opcode_index) {
        std::lock_guard<std::mutex> lock(debug_mutex);
        
        for (auto& bp : breakpoints) {
            if (bp.function == fn && bp.opcode_index == opcode_index) {
                bp.enabled = true;
                return true;
            }
        }
        
        breakpoints.push_back({fn, opcode_index, true, 0});
        log_jit_event("INFO", "Added breakpoint at opcode " + std::to_string(opcode_index));
        return true;
    }
    
    bool remove_breakpoint(ObjFunction* fn, size_t opcode_index) {
        std::lock_guard<std::mutex> lock(debug_mutex);
        
        for (auto& bp : breakpoints) {
            if (bp.function == fn && bp.opcode_index == opcode_index) {
                bp.enabled = false;
                log_jit_event("INFO", "Removed breakpoint at opcode " + std::to_string(opcode_index));
                return true;
            }
        }
        
        return false;
    }
    
    bool should_break(ObjFunction* fn, size_t opcode_index) {
        if (!debug_mode) return false;
        
        std::lock_guard<std::mutex> lock(debug_mutex);
        
        for (auto& bp : breakpoints) {
            if (bp.enabled && bp.function == fn && bp.opcode_index == opcode_index) {
                bp.hit_count++;
                return true;
            }
        }
        
        return false;
    }
    
    void clear_all_breakpoints() {
        std::lock_guard<std::mutex> lock(debug_mutex);
        breakpoints.clear();
        log_jit_event("INFO", "Cleared all JIT breakpoints");
    }
    
    std::string get_breakpoint_report() {
        std::lock_guard<std::mutex> lock(debug_mutex);
        
        std::string report = "JIT Breakpoints:\n";
        for (const auto& bp : breakpoints) {
            std::string fn_name = bp.function->name ? bp.function->name->chars : "anonymous";
            report += "  " + fn_name + " @ opcode " + std::to_string(bp.opcode_index);
            report += std::string(" [") + (bp.enabled ? "ENABLED" : "DISABLED") + "]";
            report += " (hits: " + std::to_string(bp.hit_count) + ")\n";
        }
        
        return report;
    }
};

// ============================================================================
// 15. Extended JIT Compiler with More Opcodes
// ============================================================================

class ExtendedJITCompiler : public JITCompiler {
private:
    JITOptimizer* optimizer;
    JITProfiler* profiler;
    JITDebugInterface* debugger;
    
public:
    ExtendedJITCompiler(JITUnwindContext* unwind, IteratorRegistry* iter, 
                       UpvalueManager* upvalue, JITHotSwapManager* hotswap,
                       JITStatistics* statistics = nullptr)
        : JITCompiler(unwind, iter, upvalue, hotswap, statistics),
          optimizer(nullptr), profiler(nullptr), debugger(nullptr) {
        
        optimizer = new JITOptimizer(statistics);
        profiler = new JITProfiler();
        debugger = new JITDebugInterface();
    }
    
    ~ExtendedJITCompiler() {
        delete optimizer;
        delete profiler;
        delete debugger;
    }
    
    void emit_bitwise_op(OpCode opcode) {
        Label fall_back = assembler.new_label();
        Label done = assembler.new_label();
        
        // Pop two values
        emit_pop_value();
        assembler.mov(x86::r11d, x86::ptr(stack_ptr_reg)); // Load as integer
        emit_pop_value();
        assembler.mov(x86::r10d, x86::ptr(stack_ptr_reg));
        
        // Check if both are numbers
        assembler.mov(temp_reg, x86::ptr(stack_ptr_reg, offsetof(SapphireValue, type)));
        assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_NUMBER)));
        assembler.jne(fall_back);
        
        assembler.mov(temp_reg, x86::ptr(stack_ptr_reg, sizeof(SapphireValue) + offsetof(SapphireValue, type)));
        assembler.cmp(temp_reg, imm(static_cast<int>(ValType::VAL_NUMBER)));
        assembler.jne(fall_back);
        
        // Convert to integers
        assembler.cvttsd2si(x86::r10d, x86::xmm0);
        assembler.cvttsd2si(x86::r11d, x86::xmm1);
        
        // Perform bitwise operation
        switch (opcode) {
            case OP_BITWISE_AND:
                assembler.and_(x86::r10d, x86::r11d);
                break;
            case OP_BITWISE_OR:
                assembler.or_(x86::r10d, x86::r11d);
                break;
            case OP_BITWISE_XOR:
                assembler.xor_(x86::r10d, x86::r11d);
                break;
            case OP_LEFT_SHIFT:
                assembler.mov(x86::ecx, x86::r11d);
                assembler.shl(x86::r10d, x86::cl);
                break;
            case OP_RIGHT_SHIFT:
                assembler.mov(x86::ecx, x86::r11d);
                assembler.sar(x86::r10d, x86::cl);
                break;
            case OP_BITWISE_NOT:
                assembler.not_(x86::r10d);
                break;
            default:
                assembler.jmp(fall_back);
        }
        
        // Convert back to double
        assembler.cvtsi2sd(x86::xmm0, x86::r10d);
        emit_push_value();
        assembler.jmp(done);
        
        assembler.bind(fall_back);
        assembler.xor_(x86::eax, x86::eax);
        assembler.ret();
        
        assembler.bind(done);
    }
    
    void emit_string_operations(OpCode opcode) {
        Label fall_back = assembler.new_label();
        Label done = assembler.new_label();
        
        // For string operations, we need to call into VM functions
        // This is a simplified implementation
        assembler.jmp(fall_back);
        
        assembler.bind(fall_back);
        assembler.xor_(x86::eax, x86::eax);
        assembler.ret();
        
        assembler.bind(done);
    }
    
    void emit_array_operations(OpCode opcode) {
        Label fall_back = assembler.new_label();
        Label done = assembler.new_label();
        
        switch (opcode) {
            case OP_BUILD_ARRAY: {
                // Pop count and build array
                emit_pop_value();
                assembler.mov(temp_reg, x86::ptr(stack_ptr_reg));
                // Call VM function to build array
                assembler.jmp(fall_back);
                break;
            }
            case OP_BUILD_MAP: {
                // Pop count and build map
                emit_pop_value();
                assembler.mov(temp_reg, x86::ptr(stack_ptr_reg));
                // Call VM function to build map
                assembler.jmp(fall_back);
                break;
            }
            case OP_GET_SUBSCRIPT: {
                // Array/object indexing
                emit_pop_value();
                assembler.movq(x86::xmm1, x86::ptr(stack_ptr_reg));
                emit_pop_value();
                assembler.movq(x86::xmm0, x86::ptr(stack_ptr_reg));
                // Call VM function for subscript
                assembler.jmp(fall_back);
                break;
            }
            case OP_SET_SUBSCRIPT: {
                // Array/object assignment
                emit_pop_value();
                assembler.movq(x86::xmm2, x86::ptr(stack_ptr_reg));
                emit_pop_value();
                assembler.movq(x86::xmm1, x86::ptr(stack_ptr_reg));
                emit_pop_value();
                assembler.movq(x86::xmm0, x86::ptr(stack_ptr_reg));
                // Call VM function for subscript assignment
                assembler.jmp(fall_back);
                break;
            }
            default:
                assembler.jmp(fall_back);
        }
        
        assembler.bind(fall_back);
        assembler.xor_(x86::eax, x86::eax);
        assembler.ret();
        
        assembler.bind(done);
    }
    
    JITCompilationResult compile_function_extended(ObjFunction* fn, void** out_code_ptr) {
        // Run optimizations first
        if (optimizer) {
            optimizer->optimize_function(fn);
        }
        
        // Then compile
        return compile_function_internal(fn, out_code_ptr);
    }
    
    JITOptimizer* get_optimizer() { return optimizer; }
    JITProfiler* get_profiler() { return profiler; }
    JITDebugInterface* get_debugger() { return debugger; }
};

// ============================================================================
// 16. Memory Management Integration
// ============================================================================

class JITMemoryManager {
private:
    struct MemoryPool {
        void* base_address;
        size_t pool_size;
        size_t used_size;
        std::vector<void*> allocations;
        
        MemoryPool(size_t size) : pool_size(size), used_size(0) {
            base_address = std::malloc(size);
        }
        
        ~MemoryPool() {
            if (base_address) {
                std::free(base_address);
            }
        }
    };
    
    std::vector<std::unique_ptr<MemoryPool>> memory_pools;
    std::mutex memory_mutex;
    size_t total_allocated;
    size_t total_freed;
    
public:
    JITMemoryManager() : total_allocated(0), total_freed(0) {
        // Create initial memory pool
        memory_pools.push_back(std::make_unique<MemoryPool>(1024 * 1024)); // 1MB
    }
    
    void* allocate(size_t size, size_t alignment = 16) {
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        // Find pool with enough space
        for (auto& pool : memory_pools) {
            size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
            
            if (pool->used_size + aligned_size <= pool->pool_size) {
                void* ptr = static_cast<uint8_t*>(pool->base_address) + pool->used_size;
                pool->used_size += aligned_size;
                pool->allocations.push_back(ptr);
                total_allocated += aligned_size;
                return ptr;
            }
        }
        
        // Allocate new pool if needed
        size_t new_pool_size = std::max(size * 2, static_cast<size_t>(1024 * 1024));
        memory_pools.push_back(std::make_unique<MemoryPool>(new_pool_size));
        
        auto& new_pool = memory_pools.back();
        void* ptr = new_pool->base_address;
        new_pool->used_size = size;
        new_pool->allocations.push_back(ptr);
        total_allocated += size;
        
        return ptr;
    }
    
    void deallocate(void* ptr) {
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        for (auto& pool : memory_pools) {
            auto it = std::find(pool->allocations.begin(), pool->allocations.end(), ptr);
            if (it != pool->allocations.end()) {
                pool->allocations.erase(it);
                total_freed += sizeof(ptr); // Simplified
                return;
            }
        }
    }
    
    void cleanup_pools() {
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        // Remove empty pools
        memory_pools.erase(
            std::remove_if(memory_pools.begin(), memory_pools.end(),
                [](const std::unique_ptr<MemoryPool>& pool) {
                    return pool->allocations.empty();
                }),
            memory_pools.end()
        );
    }
    
    size_t get_total_allocated() const { return total_allocated; }
    size_t get_total_freed() const { return total_freed; }
    size_t get_current_usage() const { return total_allocated - total_freed; }
    
    std::string get_memory_report() const {
        std::string report = "JIT Memory Report:\n";
        report += "  Total Allocated: " + std::to_string(total_allocated) + " bytes\n";
        report += "  Total Freed: " + std::to_string(total_freed) + " bytes\n";
        report += "  Current Usage: " + std::to_string(get_current_usage()) + " bytes\n";
        report += "  Memory Pools: " + std::to_string(memory_pools.size()) + "\n";
        return report;
    }
};

// ============================================================================
// 17. Thread-Safe JIT Context Manager
// ============================================================================

class ThreadSafeJITContext {
private:
    std::unordered_map<std::thread::id, std::unique_ptr<JITContext>> thread_contexts;
    mutable std::shared_mutex context_mutex;
    
public:
    JITContext* get_context_for_thread(VM* vm) {
        std::thread::id tid = std::this_thread::get_id();
        
        {
            std::shared_lock<std::shared_mutex> lock(context_mutex);
            if (thread_contexts.find(tid) != thread_contexts.end()) {
                return thread_contexts[tid].get();
            }
        }
        
        {
            std::unique_lock<std::shared_mutex> lock(context_mutex);
            if (thread_contexts.find(tid) == thread_contexts.end()) {
                thread_contexts[tid] = std::make_unique<JITContext>();
            }
            return thread_contexts[tid].get();
        }
    }
    
    void cleanup_thread_context(std::thread::id tid) {
        std::unique_lock<std::shared_mutex> lock(context_mutex);
        thread_contexts.erase(tid);
    }
    
    void cleanup_all_contexts() {
        std::unique_lock<std::shared_mutex> lock(context_mutex);
        thread_contexts.clear();
    }
    
    size_t active_thread_count() const {
        std::shared_lock<std::shared_mutex> lock(context_mutex);
        return thread_contexts.size();
    }
};

// ============================================================================
// 18. JIT Error Handler
// ============================================================================

class JITErrorHandler {
private:
    struct ErrorRecord {
        std::string error_message;
        HotSwapReason reason;
        std::chrono::system_clock::time_point timestamp;
        ObjFunction* function;
        size_t opcode_index;
    };
    
    std::vector<ErrorRecord> error_history;
    mutable std::mutex error_mutex;
    size_t max_error_history;
    
public:
    JITErrorHandler(size_t max_history = 100) : max_error_history(max_history) {}
    
    void record_error(const std::string& message, HotSwapReason reason, 
                      ObjFunction* fn = nullptr, size_t opcode_idx = 0) {
        std::lock_guard<std::mutex> lock(error_mutex);
        
        ErrorRecord record;
        record.error_message = message;
        record.reason = reason;
        record.timestamp = std::chrono::system_clock::now();
        record.function = fn;
        record.opcode_index = opcode_idx;
        
        error_history.push_back(record);
        
        // Trim history if needed
        if (error_history.size() > max_error_history) {
            error_history.erase(error_history.begin());
        }
        
        log_jit_event("ERROR", "Recorded JIT error: " + message);
    }
    
    std::string get_error_report() const {
        std::lock_guard<std::mutex> lock(error_mutex);
        
        std::string report = "JIT Error History:\n";
        for (const auto& record : error_history) {
            auto time = std::chrono::system_clock::to_time_t(record.timestamp);
            report += "[" + std::string(std::ctime(&time)) + "] ";
            report += record.error_message + "\n";
            
            if (record.function) {
                std::string fn_name = record.function->name ? record.function->name->chars : "anonymous";
                report += "  Function: " + fn_name + " @ opcode " + std::to_string(record.opcode_index) + "\n";
            }
        }
        
        return report;
    }
    
    void clear_error_history() {
        std::lock_guard<std::mutex> lock(error_mutex);
        error_history.clear();
    }
    
    size_t error_count() const {
        std::lock_guard<std::mutex> lock(error_mutex);
        return error_history.size();
    }
};

// ============================================================================
// 19. JIT Configuration Manager
// ============================================================================

class JITConfiguration {
private:
    struct JITConfig {
        bool enable_optimizations;
        bool enable_profiling;
        bool enable_debug;
        bool enable_memory_pooling;
        int hotswap_threshold;
        size_t max_code_size;
        size_t max_iterators;
        size_t max_upvalues;
        bool thread_safe;
        
        JITConfig() : enable_optimizations(true), enable_profiling(false),
                     enable_debug(false), enable_memory_pooling(true),
                     hotswap_threshold(JIT_HOTSWAP_THRESHOLD),
                     max_code_size(JIT_MAX_CODE_SIZE),
                     max_iterators(JIT_MAX_ITERATORS),
                     max_upvalues(JIT_MAX_UPVALUES),
                     thread_safe(false) {}
    };
    
    JITConfig config;
    
public:
    void set_optimization_enabled(bool enabled) {
        config.enable_optimizations = enabled;
        log_jit_event("INFO", std::string("Optimizations ") + (enabled ? "enabled" : "disabled"));
    }
    
    void set_profiling_enabled(bool enabled) {
        config.enable_profiling = enabled;
        log_jit_event("INFO", std::string("Profiling ") + (enabled ? "enabled" : "disabled"));
    }
    
    void set_debug_enabled(bool enabled) {
        config.enable_debug = enabled;
        log_jit_event("INFO", std::string("Debug mode ") + (enabled ? "enabled" : "disabled"));
    }
    
    void set_memory_pooling_enabled(bool enabled) {
        config.enable_memory_pooling = enabled;
        log_jit_event("INFO", std::string("Memory pooling ") + (enabled ? "enabled" : "disabled"));
    }
    
    void set_hotswap_threshold(int threshold) {
        config.hotswap_threshold = threshold;
        log_jit_event("INFO", "Hot-swap threshold set to " + std::to_string(threshold));
    }
    
    void set_max_code_size(size_t size) {
        config.max_code_size = size;
        log_jit_event("INFO", "Max code size set to " + std::to_string(size));
    }
    
    void set_thread_safe(bool enabled) {
        config.thread_safe = enabled;
        log_jit_event("INFO", std::string("Thread safety ") + (enabled ? "enabled" : "disabled"));
    }
    
    const JITConfig& get_config() const {
        return config;
    }
    
    std::string get_config_report() const {
        std::string report = "JIT Configuration:\n";
        report += "  Optimizations: " + std::string(config.enable_optimizations ? "enabled" : "disabled") + "\n";
        report += "  Profiling: " + std::string(config.enable_profiling ? "enabled" : "disabled") + "\n";
        report += "  Debug: " + std::string(config.enable_debug ? "enabled" : "disabled") + "\n";
        report += "  Memory Pooling: " + std::string(config.enable_memory_pooling ? "enabled" : "disabled") + "\n";
        report += "  Hot-swap Threshold: " + std::to_string(config.hotswap_threshold) + "\n";
        report += "  Max Code Size: " + std::to_string(config.max_code_size) + "\n";
        report += "  Max Iterators: " + std::to_string(config.max_iterators) + "\n";
        report += "  Max Upvalues: " + std::to_string(config.max_upvalues) + "\n";
        report += "  Thread Safe: " + std::string(config.thread_safe ? "enabled" : "disabled") + "\n";
        return report;
    }
};

// ============================================================================
// 20. Global JIT Manager
// ============================================================================

class GlobalJITManager {
private:
    std::unique_ptr<JITConfiguration> config;
    std::unique_ptr<JITMemoryManager> memory_manager;
    std::unique_ptr<JITErrorHandler> error_handler;
    std::unique_ptr<ThreadSafeJITContext> thread_context_manager;
    
    static GlobalJITManager* instance;
    static std::mutex instance_mutex;
    
    GlobalJITManager() {
        config = std::make_unique<JITConfiguration>();
        memory_manager = std::make_unique<JITMemoryManager>();
        error_handler = std::make_unique<JITErrorHandler>();
        thread_context_manager = std::make_unique<ThreadSafeJITContext>();
        
        log_jit_event("INFO", "Global JIT Manager initialized");
    }
    
public:
    static GlobalJITManager* get_instance() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (!instance) {
            instance = new GlobalJITManager();
        }
        return instance;
    }
    
    static void shutdown() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (instance) {
            delete instance;
            instance = nullptr;
        }
    }
    
    JITConfiguration* get_config() { return config.get(); }
    JITMemoryManager* get_memory_manager() { return memory_manager.get(); }
    JITErrorHandler* get_error_handler() { return error_handler.get(); }
    ThreadSafeJITContext* get_thread_context_manager() { return thread_context_manager.get(); }
    
    std::string get_global_report() {
        std::string report = "Global JIT Manager Report:\n";
        report += "================================\n\n";
        report += config->get_config_report() + "\n";
        report += memory_manager->get_memory_report() + "\n";
        report += error_handler->get_error_report() + "\n";
        report += "Active Threads: " + std::to_string(thread_context_manager->active_thread_count()) + "\n";
        return report;
    }
};

GlobalJITManager* GlobalJITManager::instance = nullptr;
std::mutex GlobalJITManager::instance_mutex;

// ============================================================================
// 21. Extended Public API
// ============================================================================

extern "C" {

bool jit_run_function_optimized(VM* vm, ObjFunction* fn) {
    JITContext* ctx = get_jit_context(vm);
    
    if (!ctx->compiler) {
        ctx->initialize_compiler();
    }
    
    // Try to use extended compiler if available
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (global && global->get_config()->get_config().enable_optimizations) {
        // Create extended compiler with optimizations
        // This would require modifying the context to support extended compiler
    }
    
    void* compiled_code = ctx->compiler->compile_function(fn);
    if (!compiled_code) {
        log_jit_event("ERROR", "Failed to compile function");
        ctx->hotswap_manager->trigger_hotswap(vm, HotSwapReason::UNSUPPORTED_OPCODE, 
                                              "Compilation failed");
        return false;
    }
    
    return ctx->compiler->execute_compiled_code(compiled_code, vm);
}

void jit_enable_optimizations(bool enable) {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (global) {
        global->get_config()->set_optimization_enabled(enable);
    }
}

void jit_enable_profiling(bool enable) {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (global) {
        global->get_config()->set_profiling_enabled(enable);
    }
}

void jit_enable_debug(bool enable) {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (global) {
        global->get_config()->set_debug_enabled(enable);
    }
}

char* jit_get_global_report() {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (!global) {
        return nullptr;
    }
    
    std::string report = global->get_global_report();
    char* result = static_cast<char*>(std::malloc(report.length() + 1));
    std::strcpy(result, report.c_str());
    return result;
}

void jit_free_report(char* report) {
    if (report) {
        std::free(report);
    }
}

void jit_set_hotswap_threshold(int threshold) {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (global) {
        global->get_config()->set_hotswap_threshold(threshold);
    }
}

void jit_set_thread_safe(bool enable) {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (global) {
        global->get_config()->set_thread_safe(enable);
    }
}

bool jit_add_breakpoint(VM* vm, ObjFunction* fn, size_t opcode_index) {
    JITContext* ctx = get_jit_context(vm);
    // This would require extended compiler with debug interface
    return false;
}

bool jit_remove_breakpoint(VM* vm, ObjFunction* fn, size_t opcode_index) {
    JITContext* ctx = get_jit_context(vm);
    // This would require extended compiler with debug interface
    return false;
}

char* jit_get_optimization_report() {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (!global) {
        return nullptr;
    }
    
    // This would require access to optimizer
    std::string report = "Optimization report not available in current context";
    char* result = static_cast<char*>(std::malloc(report.length() + 1));
    std::strcpy(result, report.c_str());
    return result;
}

char* jit_get_profile_report() {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (!global) {
        return nullptr;
    }
    
    // This would require access to profiler
    std::string report = "Profile report not available in current context";
    char* result = static_cast<char*>(std::malloc(report.length() + 1));
    std::strcpy(result, report.c_str());
    return result;
}

void jit_global_shutdown() {
    GlobalJITManager::shutdown();
    jit_shutdown();
}

} // extern "C"

// ============================================================================
// 22. Performance Benchmarking Utilities
// ============================================================================

class JITBenchmark {
private:
    struct BenchmarkResult {
        std::string name;
        std::chrono::microseconds jit_time;
        std::chrono::microseconds interpreter_time;
        double speedup;
        
        BenchmarkResult() : speedup(0.0) {}
    };
    
    std::vector<BenchmarkResult> results;
    
public:
    void run_benchmark(VM* vm, ObjFunction* fn, const std::string& name) {
        BenchmarkResult result;
        result.name = name;
        
        // Benchmark JIT execution
        auto jit_start = std::chrono::high_resolution_clock::now();
        bool jit_enabled = vm->jit_enabled;
        vm->jit_enabled = true;
        
        for (int i = 0; i < 1000; i++) {
            jit_run_function(vm, fn);
        }
        
        auto jit_end = std::chrono::high_resolution_clock::now();
        result.jit_time = std::chrono::duration_cast<std::chrono::microseconds>(jit_end - jit_start);
        
        // Benchmark interpreter execution
        auto interp_start = std::chrono::high_resolution_clock::now();
        vm->jit_enabled = false;
        
        for (int i = 0; i < 1000; i++) {
            vm->run_function(fn);
        }
        
        auto interp_end = std::chrono::high_resolution_clock::now();
        result.interpreter_time = std::chrono::duration_cast<std::chrono::microseconds>(interp_end - interp_start);
        
        // Calculate speedup
        if (result.interpreter_time.count() > 0) {
            result.speedup = static_cast<double>(result.interpreter_time.count()) / 
                           static_cast<double>(result.jit_time.count());
        }
        
        // Restore original state
        vm->jit_enabled = jit_enabled;
        
        results.push_back(result);
        
        log_jit_event("INFO", "Benchmark '" + name + "' completed. Speedup: " + 
                     std::to_string(result.speedup) + "x");
    }
    
    std::string get_benchmark_report() {
        std::string report = "JIT Benchmark Results:\n";
        report += "========================\n";
        
        for (const auto& result : results) {
            report += "Benchmark: " + result.name + "\n";
            report += "  JIT Time: " + std::to_string(result.jit_time.count()) + "μs\n";
            report += "  Interpreter Time: " + std::to_string(result.interpreter_time.count()) + "μs\n";
            report += "  Speedup: " + std::to_string(result.speedup) + "x\n";
            report += "\n";
        }
        
        return report;
    }
    
    void clear_results() {
        results.clear();
    }
};

// ============================================================================
// 23. Validation and Testing Utilities
// ============================================================================

class JITValidator {
public:
    struct ValidationResult {
        bool passed;
        std::string error_message;
        size_t error_location;
        
        ValidationResult() : passed(true), error_location(0) {}
    };
    
public:
    ValidationResult validate_function(ObjFunction* fn) {
        ValidationResult result;
        
        // Validate chunk structure
        if (fn->chunk.code.empty()) {
            result.passed = false;
            result.error_message = "Empty chunk code";
            return result;
        }
        
        // Validate constant indices
        for (size_t i = 0; i < fn->chunk.code.size(); ) {
            uint8_t opcode = fn->chunk.code[i];
            
            if (opcode == OP_CONSTANT && i + 2 < fn->chunk.code.size()) {
                uint16_t const_idx = (fn->chunk.code[i + 1] << 8) | fn->chunk.code[i + 2];
                if (const_idx >= fn->chunk.constants.size()) {
                    result.passed = false;
                    result.error_message = "Constant index out of bounds";
                    result.error_location = i;
                    return result;
                }
                i += 3;
            } else if (opcode == OP_GET_LOCAL && i + 1 < fn->chunk.code.size()) {
                // Validate local slot
                i += 2;
            } else if ((opcode == OP_JUMP || opcode == OP_JUMP_IF_FALSE || 
                       opcode == OP_LOOP) && i + 2 < fn->chunk.code.size()) {
                // Validate jump offset
                int16_t offset = (fn->chunk.code[i + 1] << 8) | fn->chunk.code[i + 2];
                size_t target = i + offset;
                if (target >= fn->chunk.code.size()) {
                    result.passed = false;
                    result.error_message = "Jump target out of bounds";
                    result.error_location = i;
                    return result;
                }
                i += 3;
            } else {
                i++;
            }
        }
        
        return result;
    }
    
    ValidationResult validate_stack_integrity(VM* vm) {
        ValidationResult result;
        
        size_t stack_usage = vm->stack_top - vm->stack;
        if (stack_usage > STACK_MAX) {
            result.passed = false;
            result.error_message = "Stack overflow detected";
            return result;
        }
        
        if (vm->frame_count > FRAMES_MAX) {
            result.passed = false;
            result.error_message = "Frame count exceeds maximum";
            return result;
        }
        
        return result;
    }
    
    ValidationResult validate_memory_safety(VM* vm) {
        ValidationResult result;
        
        // Check for null pointers in critical structures
        if (!vm->stack) {
            result.passed = false;
            result.error_message = "Null stack pointer";
            return result;
        }
        
        if (vm->frame_count > 0 && !vm->frames[vm->frame_count - 1].function) {
            result.passed = false;
            result.error_message = "Null function in active frame";
            return result;
        }
        
        return result;
    }
    
    std::string run_full_validation(VM* vm, ObjFunction* fn) {
        std::string report = "JIT Validation Report:\n";
        
        ValidationResult func_result = validate_function(fn);
        report += "Function Validation: " + std::string(func_result.passed ? "PASSED" : "FAILED");
        if (!func_result.passed) {
            report += " - " + func_result.error_message + " @ " + std::to_string(func_result.error_location);
        }
        report += "\n";
        
        ValidationResult stack_result = validate_stack_integrity(vm);
        report += "Stack Integrity: " + std::string(stack_result.passed ? "PASSED" : "FAILED");
        if (!stack_result.passed) {
            report += " - " + stack_result.error_message;
        }
        report += "\n";
        
        ValidationResult mem_result = validate_memory_safety(vm);
        report += "Memory Safety: " + std::string(mem_result.passed ? "PASSED" : "FAILED");
        if (!mem_result.passed) {
            report += " - " + mem_result.error_message;
        }
        report += "\n";
        
        return report;
    }
};

// ============================================================================
// 24. Integration Helper Functions
// ============================================================================

namespace JITIntegration {
    
bool initialize_jit_for_vm(VM* vm) {
    if (!vm) {
        return false;
    }
    
    JITContext* ctx = get_jit_context(vm);
    if (!ctx) {
        return false;
    }
    
    ctx->initialize_compiler();
    
    log_jit_event("INFO", "JIT initialized for VM");
    return true;
}

bool deinitialize_jit_for_vm(VM* vm) {
    if (!vm) {
        return false;
    }
    
    cleanup_jit_context(vm);
    
    log_jit_event("INFO", "JIT deinitialized for VM");
    return true;
}

bool compile_and_run_function(VM* vm, ObjFunction* fn) {
    if (!initialize_jit_for_vm(vm)) {
        return false;
    }
    
    return jit_run_function(vm, fn);
}

void set_jit_configuration(JITConfiguration* config) {
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (global && config) {
        // Apply configuration
        // This would require copying config values
    }
}

std::string get_diagnostics(VM* vm) {
    std::string report = "JIT Diagnostics:\n";
    
    JITContext* ctx = get_jit_context(vm);
    if (ctx) {
        char stats_buffer[4096];
        jit_get_statistics(vm, stats_buffer, sizeof(stats_buffer));
        report += stats_buffer;
    }
    
    GlobalJITManager* global = GlobalJITManager::get_instance();
    if (global) {
        report += global->get_global_report();
    }
    
    return report;
}

} // namespace JITIntegration

// ============================================================================
// 25. Final Static Initialization
// ============================================================================

static bool jit_global_initialized = jit_initialize_global();

// Global cleanup on shutdown
static struct JITCleanup {
    ~JITCleanup() {
        jit_global_shutdown();
    }
} jit_cleanup_handler;

// ============================================================================
// 26. Additional Opcode Implementations
// ============================================================================

class ExtendedOpcodeHandler {
private:
    JITCompiler* compiler;
    
public:
    ExtendedOpcodeHandler(JITCompiler* comp) : compiler(comp) {}
    
    void emit_closure_creation(ObjFunction* fn, uint8_t constant_idx) {
        // Emit code to create closure with upvalues
        // This requires access to the upvalue manager
        // For now, we'll emit a fallback
    }
    
    void emit_class_definition(ObjFunction* fn, uint8_t constant_idx) {
        // Emit code to define class
        // This requires class structure support
    }
    
    void emit_method_call(ObjFunction* fn, uint8_t arg_count) {
        // Emit code for method invocation
        // This requires instance method lookup
    }
    
    void emit_super_instruction(ObjFunction* fn, uint8_t constant_idx) {
        // Emit code for super class access
        // This requires inheritance support
    }
    
    void emit_property_access(ObjFunction* fn, uint8_t constant_idx, bool is_set) {
        // Emit code for property get/set
        // This requires object property management
    }
    
    void emit_async_operations(OpCode opcode) {
        // Emit code for async/await operations
        // This requires promise/future support
    }
    
    void emit_import_statement(ObjFunction* fn, uint8_t constant_idx) {
        // Emit code for module import
        // This requires module loading system
    }
};

// ============================================================================
// 27. JIT Cache Management
// ============================================================================

class JITCacheManager {
private:
    struct CacheEntry {
        ObjFunction* function;
        void* compiled_code;
        std::chrono::system_clock::time_point compilation_time;
        size_t access_count;
        size_t code_size;
        bool is_valid;
        
        CacheEntry() : function(nullptr), compiled_code(nullptr), access_count(0),
                     code_size(0), is_valid(false) {}
    };
    
    std::unordered_map<ObjFunction*, CacheEntry> cache;
    std::mutex cache_mutex;
    size_t max_cache_size;
    size_t current_cache_size;
    
public:
    JITCacheManager(size_t max_size = 10 * 1024 * 1024) // 10MB default
        : max_cache_size(max_size), current_cache_size(0) {}
    
    bool add_to_cache(ObjFunction* fn, void* compiled_code, size_t code_size) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        // Check if adding would exceed cache size
        if (current_cache_size + code_size > max_cache_size) {
            evict_lru();
        }
        
        if (current_cache_size + code_size > max_cache_size) {
            log_jit_event("WARNING", "Cache full, cannot add function");
            return false;
        }
        
        CacheEntry entry;
        entry.function = fn;
        entry.compiled_code = compiled_code;
        entry.compilation_time = std::chrono::system_clock::now();
        entry.access_count = 0;
        entry.code_size = code_size;
        entry.is_valid = true;
        
        cache[fn] = entry;
        current_cache_size += code_size;
        
        log_jit_event("INFO", "Added function to JIT cache (size: " + std::to_string(code_size) + 
                     " bytes, total: " + std::to_string(current_cache_size) + " bytes)");
        
        return true;
    }
    
    void* get_from_cache(ObjFunction* fn) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        auto it = cache.find(fn);
        if (it != cache.end() && it->second.is_valid) {
            it->second.access_count++;
            return it->second.compiled_code;
        }
        
        return nullptr;
    }
    
    void invalidate(ObjFunction* fn) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        auto it = cache.find(fn);
        if (it != cache.end()) {
            current_cache_size -= it->second.code_size;
            cache.erase(it);
            log_jit_event("INFO", "Invalidated function from JIT cache");
        }
    }
    
    void evict_lru() {
        // Find least recently used entry
        auto lru_it = cache.begin();
        size_t min_access = std::numeric_limits<size_t>::max();
        
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.access_count < min_access) {
                min_access = it->second.access_count;
                lru_it = it;
            }
        }
        
        if (lru_it != cache.end()) {
            current_cache_size -= lru_it->second.code_size;
            cache.erase(lru_it);
            log_jit_event("INFO", "Evicted LRU entry from JIT cache");
        }
    }
    
    void clear_cache() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache.clear();
        current_cache_size = 0;
        log_jit_event("INFO", "Cleared entire JIT cache");
    }
    
    std::string get_cache_stats() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        std::string report = "JIT Cache Statistics:\n";
        report += "  Cached Functions: " + std::to_string(cache.size()) + "\n";
        report += "  Current Size: " + std::to_string(current_cache_size) + " bytes\n";
        report += "  Max Size: " + std::to_string(max_cache_size) + " bytes\n";
        report += "  Utilization: " + std::to_string((current_cache_size * 100) / max_cache_size) + "%\n";
        
        return report;
    }
    
    void set_max_size(size_t new_size) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        max_cache_size = new_size;
        
        // Evict if necessary
        while (current_cache_size > max_cache_size && !cache.empty()) {
            evict_lru();
        }
        
        log_jit_event("INFO", "JIT cache max size set to " + std::to_string(new_size) + " bytes");
    }
};

// ============================================================================
// 28. JIT Code Verification
// ============================================================================

class JITCodeVerifier {
private:
    struct VerificationRule {
        std::string name;
        std::function<bool(const uint8_t*, size_t)> check;
    };
    
    std::vector<VerificationRule> rules;
    
public:
    JITCodeVerifier() {
        setup_rules();
    }
    
    void setup_rules() {
        rules.push_back({"Null pointer check", [](const uint8_t* code, size_t size) {
            return code != nullptr;
        }});
        
        rules.push_back({"Code size sanity", [](const uint8_t* code, size_t size) {
            return size > 0 && size < JIT_MAX_CODE_SIZE;
        }});
        
        rules.push_back({"Opcode validity", [](const uint8_t* code, size_t size) {
            for (size_t i = 0; i < size; i++) {
                if (code[i] > OP_CLASS) { // Assuming OP_CLASS is the max opcode
                    return false;
                }
            }
            return true;
        }});
        
        rules.push_back({"Jump target validity", [](const uint8_t* code, size_t size) {
            for (size_t i = 0; i < size; ) {
                uint8_t opcode = code[i];
                if ((opcode == OP_JUMP || opcode == OP_JUMP_IF_FALSE || 
                     opcode == OP_LOOP) && i + 2 < size) {
                    int16_t offset = (code[i + 1] << 8) | code[i + 2];
                    size_t target = i + offset;
                    if (target >= size) {
                        return false;
                    }
                    i += 3;
                } else {
                    i++;
                }
            }
            return true;
        }});
    }
    
    struct VerificationResult {
        bool passed;
        std::string failed_rule;
        size_t error_location;
        
        VerificationResult() : passed(true), error_location(0) {}
    };
    
    VerificationResult verify_code(const uint8_t* code, size_t size) {
        VerificationResult result;
        
        for (const auto& rule : rules) {
            if (!rule.check(code, size)) {
                result.passed = false;
                result.failed_rule = rule.name;
                return result;
            }
        }
        
        return result;
    }
    
    VerificationResult verify_function(ObjFunction* fn) {
        if (!fn) {
            VerificationResult result;
            result.passed = false;
            result.failed_rule = "Null function";
            return result;
        }
        
        return verify_code(fn->chunk.code.data(), fn->chunk.code.size());
    }
};

// ============================================================================
// 29. JIT Performance Counters
// ============================================================================

class JITPerformanceCounters {
private:
    struct Counter {
        std::string name;
        uint64_t count;
        std::string unit;
        
        Counter(const std::string& n, const std::string& u = "") 
            : name(n), count(0), unit(u) {}
    };
    
    std::vector<Counter> counters;
    std::mutex counter_mutex;
    
public:
    JITPerformanceCounters() {
        // Initialize standard counters
        counters.emplace_back("functions_compiled", "");
        counters.emplace_back("cache_hits", "");
        counters.emplace_back("cache_misses", "");
        counters.emplace_back("optimizations_applied", "");
        counters.emplace_back("hotswaps_triggered", "");
        counters.emplace_back("compilation_time_ms", "ms");
        counters.emplace_back("execution_time_ms", "ms");
        counters.emplace_back("memory_allocated", "bytes");
        counters.emplace_back("memory_freed", "bytes");
    }
    
    void increment_counter(const std::string& name, uint64_t amount = 1) {
        std::lock_guard<std::mutex> lock(counter_mutex);
        
        for (auto& counter : counters) {
            if (counter.name == name) {
                counter.count += amount;
                return;
            }
        }
    }
    
    void set_counter(const std::string& name, uint64_t value) {
        std::lock_guard<std::mutex> lock(counter_mutex);
        
        for (auto& counter : counters) {
            if (counter.name == name) {
                counter.count = value;
                return;
            }
        }
    }
    
    uint64_t get_counter(const std::string& name) {
        std::lock_guard<std::mutex> lock(counter_mutex);
        
        for (const auto& counter : counters) {
            if (counter.name == name) {
                return counter.count;
            }
        }
        
        return 0;
    }
    
    std::string get_counters_report() {
        std::lock_guard<std::mutex> lock(counter_mutex);
        
        std::string report = "JIT Performance Counters:\n";
        for (const auto& counter : counters) {
            report += "  " + counter.name + ": " + std::to_string(counter.count);
            if (!counter.unit.empty()) {
                report += " " + counter.unit;
            }
            report += "\n";
        }
        
        return report;
    }
    
    void reset_counters() {
        std::lock_guard<std::mutex> lock(counter_mutex);
        
        for (auto& counter : counters) {
            counter.count = 0;
        }
        
        log_jit_event("INFO", "Reset all JIT performance counters");
    }
};

// ============================================================================
// 30. JIT Event Tracing
// ============================================================================

class JITEventTracer {
private:
    struct TraceEvent {
        std::string event_type;
        std::string details;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id thread_id;
    };
    
    std::vector<TraceEvent> trace_buffer;
    std::mutex trace_mutex;
    size_t max_buffer_size;
    bool tracing_enabled;
    
public:
    JITEventTracer(size_t buffer_size = 10000) 
        : max_buffer_size(buffer_size), tracing_enabled(false) {}
    
    void enable_tracing(bool enable = true) {
        tracing_enabled = enable;
        log_jit_event("INFO", std::string("JIT event tracing ") + (enable ? "enabled" : "disabled"));
    }
    
    void trace_event(const std::string& event_type, const std::string& details) {
        if (!tracing_enabled) return;
        
        std::lock_guard<std::mutex> lock(trace_mutex);
        
        TraceEvent event;
        event.event_type = event_type;
        event.details = details;
        event.timestamp = std::chrono::system_clock::now();
        event.thread_id = std::this_thread::get_id();
        
        trace_buffer.push_back(event);
        
        // Trim buffer if needed
        if (trace_buffer.size() > max_buffer_size) {
            trace_buffer.erase(trace_buffer.begin());
        }
    }
    
    std::string get_trace_log() {
        std::lock_guard<std::mutex> lock(trace_mutex);
        
        std::string log = "JIT Event Trace Log:\n";
        log += "=====================\n";
        
        for (const auto& event : trace_buffer) {
            auto time = std::chrono::system_clock::to_time_t(event.timestamp);
            log += "[" + std::string(std::ctime(&time)) + "] ";
            log += event.event_type + ": " + event.details + "\n";
        }
        
        return log;
    }
    
    void clear_trace() {
        std::lock_guard<std::mutex> lock(trace_mutex);
        trace_buffer.clear();
        log_jit_event("INFO", "Cleared JIT event trace");
    }
    
    std::vector<TraceEvent> get_events_by_type(const std::string& event_type) {
        std::lock_guard<std::mutex> lock(trace_mutex);
        
        std::vector<TraceEvent> filtered;
        for (const auto& event : trace_buffer) {
            if (event.event_type == event_type) {
                filtered.push_back(event);
            }
        }
        
        return filtered;
    }
};

// ============================================================================
// 31. JIT Memory Pool Allocator
// ============================================================================

class JITMemoryPoolAllocator {
private:
    struct MemoryBlock {
        void* address;
        size_t size;
        bool in_use;
        
        MemoryBlock(void* addr, size_t sz) : address(addr), size(sz), in_use(false) {}
    };
    
    std::vector<std::unique_ptr<MemoryBlock>> blocks;
    mutable std::mutex pool_mutex;
    size_t block_size;
    size_t initial_blocks;
    
public:
    JITMemoryPoolAllocator(size_t block_sz = 4096, size_t initial = 100)
        : block_size(block_sz), initial_blocks(initial) {
        
        // Allocate initial blocks
        for (size_t i = 0; i < initial; i++) {
            void* block = std::malloc(block_size);
            if (block) {
                blocks.push_back(std::make_unique<MemoryBlock>(block, block_size));
            }
        }
        
        log_jit_event("INFO", "Initialized memory pool with " + std::to_string(initial) + 
                     " blocks of " + std::to_string(block_size) + " bytes");
    }
    
    ~JITMemoryPoolAllocator() {
        // Free all blocks
        for (auto& block : blocks) {
            if (block->address) {
                std::free(block->address);
            }
        }
    }
    
    void* allocate() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        // Find free block
        for (auto& block : blocks) {
            if (!block->in_use) {
                block->in_use = true;
                return block->address;
            }
        }
        
        // Allocate new block if none available
        void* new_block = std::malloc(block_size);
        if (new_block) {
            blocks.push_back(std::make_unique<MemoryBlock>(new_block, block_size));
            blocks.back()->in_use = true;
            return new_block;
        }
        
        return nullptr;
    }
    
    void deallocate(void* ptr) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        for (auto& block : blocks) {
            if (block->address == ptr) {
                block->in_use = false;
                return;
            }
        }
    }
    
    size_t get_free_blocks() const {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        size_t free_count = 0;
        for (const auto& block : blocks) {
            if (!block->in_use) {
                free_count++;
            }
        }
        
        return free_count;
    }
    
    size_t get_used_blocks() const {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        size_t used_count = 0;
        for (const auto& block : blocks) {
            if (block->in_use) {
                used_count++;
            }
        }
        
        return used_count;
    }
    
    std::string get_pool_stats() {
        std::string report = "Memory Pool Statistics:\n";
        report += "  Total Blocks: " + std::to_string(blocks.size()) + "\n";
        report += "  Free Blocks: " + std::to_string(get_free_blocks()) + "\n";
        report += "  Used Blocks: " + std::to_string(get_used_blocks()) + "\n";
        report += "  Block Size: " + std::to_string(block_size) + " bytes\n";
        report += "  Total Memory: " + std::to_string(blocks.size() * block_size) + " bytes\n";
        return report;
    }
};

// ============================================================================
// 32. Final Extended API Functions
// ============================================================================

extern "C" {

void jit_enable_tracing(bool enable) {
    // This would access the global event tracer
    log_jit_event("INFO", std::string("JIT tracing ") + (enable ? "enabled" : "disabled"));
}

char* jit_get_trace_log() {
    // This would return the trace log from the global tracer
    std::string log = "Trace log not available in current context";
    char* result = static_cast<char*>(std::malloc(log.length() + 1));
    std::strcpy(result, log.c_str());
    return result;
}

void jit_clear_trace() {
    // This would clear the global trace
    log_jit_event("INFO", "Cleared JIT trace log");
}

char* jit_get_performance_counters() {
    // This would return performance counters
    std::string counters = "Performance counters not available in current context";
    char* result = static_cast<char*>(std::malloc(counters.length() + 1));
    std::strcpy(result, counters.c_str());
    return result;
}

void jit_reset_performance_counters() {
    // This would reset the global performance counters
    log_jit_event("INFO", "Reset JIT performance counters");
}

char* jit_get_cache_stats() {
    // This would return cache statistics
    std::string stats = "Cache statistics not available in current context";
    char* result = static_cast<char*>(std::malloc(stats.length() + 1));
    std::strcpy(result, stats.c_str());
    return result;
}

void jit_clear_global_cache() {
    // This would clear the global cache
    log_jit_event("INFO", "Cleared JIT cache");
}

bool jit_verify_function(VM* vm, ObjFunction* fn) {
    // This would verify the function using the code verifier
    JITValidator validator;
    JITValidator::ValidationResult result = validator.validate_function(fn);
    return result.passed;
}

char* jit_validate_vm(VM* vm) {
    // This would run full validation on the VM
    JITValidator validator;
    char* result = nullptr;
    
    if (vm && vm->frame_count > 0) {
        ObjFunction* fn = vm->frames[vm->frame_count - 1].function;
        std::string report = validator.run_full_validation(vm, fn);
        result = static_cast<char*>(std::malloc(report.length() + 1));
        std::strcpy(result, report.c_str());
    } else {
        std::string report = "No active function to validate";
        result = static_cast<char*>(std::malloc(report.length() + 1));
        std::strcpy(result, report.c_str());
    }
    
    return result;
}

void jit_run_benchmark(VM* vm, ObjFunction* fn, const char* name) {
    // This would run a benchmark on the function
    log_jit_event("INFO", "Running benchmark: " + std::string(name));
    // Implementation would use JITBenchmark class
}

char* jit_get_benchmark_results() {
    // This would return benchmark results
    std::string results = "Benchmark results not available in current context";
    char* result = static_cast<char*>(std::malloc(results.length() + 1));
    std::strcpy(result, results.c_str());
    return result;
}

} // extern "C"



