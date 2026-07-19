#ifndef SAPPHIRE_CITRINE_H
#define SAPPHIRE_CITRINE_H

#include <string>
#include <vector>
#include <iostream>

namespace citrine {

enum class WarningLevel {
    INFO,
    PEDANTIC,
    WARNING,
    ERR
};

enum class Category {
    SYNTAX,
    STYLE,
    PERFORMANCE,
    SECURITY,
    ARCHITECTURE
};

struct LinterIssue {
    int line;
    WarningLevel level;
    Category category;
    std::string message;
    std::string explanation;
    std::string correction;
    int col;
    int length;
};

enum LintMode {
    MODE_LINT,
    MODE_EXPLAIN,
    MODE_FIX,
    MODE_UNDO
};

struct FilterConfig {
    bool has_category_filter = false;
    Category category_filter;
    bool has_level_filter = false;
    WarningLevel level_filter;
};

void run_lint(const std::string& filepath, LintMode mode, const FilterConfig& config = FilterConfig());

void export_errors(const std::vector<LinterIssue>& issues, const std::string& filepath);

} // namespace citrine

#endif // SAPPHIRE_CITRINE_H
