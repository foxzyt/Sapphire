#ifndef SAPPHIRE_QUARTZ_H
#define SAPPHIRE_QUARTZ_H

#include <string>
#include <vector>

namespace quartz {

struct BenchmarkResult {
    std::string name;
    std::string category;
    double ops_per_sec = 0.0;
    double mean_latency_ns = 0.0;
    double std_dev_pct = 0.0;
    size_t memory_allocated_bytes = 0;
};

struct BenchmarkDef {
    std::string name;
    std::string category;
    std::string description;
    std::string code;
};

struct QuartzConfig {
    int duration_ms = 1000;
};

void list_benchmarks();
void run_benchmarks(const std::string& target, const QuartzConfig& config);
void compare_benchmarks(const std::string& b1, const std::string& b2, const QuartzConfig& config);

} // namespace quartz

#endif // SAPPHIRE_QUARTZ_H
