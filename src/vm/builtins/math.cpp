#include "builtins.h"
#include "../object.h"
#include "../value.h"

#include <cmath>
#include <random>

static std::random_device rd;
static std::mt19937 gen(rd());

SapphireValue native_math_tan(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::tan(args[0].as.number);
}

SapphireValue native_math_asin(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::asin(args[0].as.number);
}

SapphireValue native_math_acos(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::acos(args[0].as.number);
}

SapphireValue native_math_atan(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::atan(args[0].as.number);
}

SapphireValue native_math_atan2(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || args[0].type != ValType::VAL_NUMBER || args[1].type != ValType::VAL_NUMBER) return 0.0;
    return std::atan2(args[0].as.number, args[1].as.number);
}

SapphireValue native_math_sinh(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::sinh(args[0].as.number);
}

SapphireValue native_math_cosh(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::cosh(args[0].as.number);
}

SapphireValue native_math_tanh(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::tanh(args[0].as.number);
}

SapphireValue native_math_exp(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::exp(args[0].as.number);
}

SapphireValue native_math_log10(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::log10(args[0].as.number);
}

SapphireValue native_math_trunc(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::trunc(args[0].as.number);
}

SapphireValue native_math_round(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::round(args[0].as.number);
}

SapphireValue native_math_sqrt(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: sqrt() expects 1 argument." << std::endl;
        }
        return {};
    }
    if (args[0].type != ValType::VAL_NUMBER) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Argument for sqrt() must be a number." << std::endl;
        }
        return {};
    }
    double number = args[0].as.number;
    return sqrt(number);
}

SapphireValue native_math_rand(int arg_count, SapphireValue* args) {
    if (arg_count > 2) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Math.rand() expects 0, 1 or 2 arguments." << std::endl;
        }
        return {};
    }

    double min_val = 0.0;
    double max_val = 1.0;

    if (arg_count == 1) {
        if (args[0].type != ValType::VAL_NUMBER) {
            if (!g_current_vm->soft_mode) {
                std::cerr << "Runtime Error: Math.rand(max) expects a number for max value." << std::endl;
            }
            return {};
        }
        max_val = args[0].as.number;
    } else if (arg_count == 2) {
        if (args[0].type != ValType::VAL_NUMBER || args[1].type != ValType::VAL_NUMBER) {
            if (!g_current_vm->soft_mode) {
                std::cerr << "Runtime Error: Math.rand(min, max) expects numbers for min and max values." << std::endl;
            }
            return {};
        }
        min_val = args[0].as.number;
        max_val = args[1].as.number;
    }

    if (min_val > max_val) {
        std::swap(min_val, max_val);
    }

    std::uniform_real_distribution<double> distrib(min_val, max_val);
    return distrib(gen);
}

SapphireValue native_math_abs(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::abs(args[0].as.number);
}

SapphireValue native_math_floor(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::floor(args[0].as.number);
}

SapphireValue native_math_ceil(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::ceil(args[0].as.number);
}

SapphireValue native_math_sin(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::sin(args[0].as.number);
}

SapphireValue native_math_cos(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::cos(args[0].as.number);
}

SapphireValue native_math_log(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return 0.0;
    return std::log(args[0].as.number);
}

SapphireValue native_math_pow(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return 0.0;
    return std::pow(args[0].as.number, args[1].as.number);
}

SapphireValue native_math_min(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return args[0];
    double a = args[0].as.number;
    double b = args[1].as.number;
    return std::min(a, b);
}

SapphireValue native_math_max(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return args[0];
    double a = args[0].as.number;
    double b = args[1].as.number;
    return std::max(a, b);
}

SapphireValue native_math_clamp(int arg_count, SapphireValue* args) {
    if (arg_count < 3) return args[0];
    double v = args[0].as.number;
    double lo = args[1].as.number;
    double hi = args[2].as.number;
    return std::clamp(v, lo, hi);
}

SapphireValue native_math_lerp(int arg_count, SapphireValue* args) {
    if (arg_count < 3) return args[0];
    double a = args[0].as.number;
    double b = args[1].as.number;
    double t = args[2].as.number;
    return a + t * (b - a);
}

