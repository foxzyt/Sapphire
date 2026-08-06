#ifndef SAPPHIRE_ERROR_H
#define SAPPHIRE_ERROR_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "tokens.h"

enum class ErrorSeverity {
    WARNING,
    ERR,
    FATAL
};

enum class ErrorType {
    SYNTAX_ERROR,
    RUNTIME_ERROR,
    TYPE_ERROR,
    REFERENCE_ERROR,
    RANGE_ERROR,
    NETWORK_ERROR,
    DATABASE_ERROR,
    UI_ERROR,
    INTERNAL_ERROR
};

struct SourceLocation {
    int line;
    int column;
    int length;
    std::string file;
    std::string source_line;
};

struct ErrorContext {
    std::string description;
    std::string value;
    std::string type;
    int depth; // For nested error context
};

struct FixSuggestion {
    std::string description;
    std::string code_snippet;
    int priority; // 1-10, higher is more important
};

class SapphireError {
public:
    std::string message;
    std::string technical_message;
    ErrorSeverity severity;
    ErrorType type;
    SourceLocation location;
    std::vector<ErrorContext> context;
    std::vector<FixSuggestion> suggestions;
    std::string stack_trace;

    SapphireError(ErrorType error_type, const std::string& user_message, 
                  const std::string& tech_message,
                  const SourceLocation& loc, ErrorSeverity sev = ErrorSeverity::ERR)
        : type(error_type), message(user_message),
          technical_message(tech_message), location(loc), severity(sev) {}

    void add_context(const std::string& desc, const std::string& val = "", const std::string& typ = "") {
        context.push_back({desc, val, typ});
    }

    void add_suggestion(const std::string& desc, const std::string& code, int priority = 5) {
        suggestions.push_back({desc, code, priority});
    }

    std::string format() const;
    std::string format_with_context() const;
};

class ErrorHandler {
private:
    std::vector<std::shared_ptr<SapphireError>> errors;
    bool verbose;
    bool show_suggestions;

public:
    ErrorHandler(bool verb = true, bool suggest = true) 
        : verbose(verb), show_suggestions(suggest) {}

    void report_error(std::shared_ptr<SapphireError> error);
    void report_warning(std::shared_ptr<SapphireError> error);
    void clear();
    bool has_errors() const;
    bool has_warnings() const;
    std::vector<std::shared_ptr<SapphireError>> get_errors() const;
    std::vector<std::shared_ptr<SapphireError>> get_warnings() const;
    
    void set_verbose(bool v) { verbose = v; }
    void set_show_suggestions(bool s) { show_suggestions = s; }
};

// Error code constants
namespace ErrorCodes {
    // Syntax errors (SYN_001 - SYN_099)
    constexpr const char* SYN_MISSING_SEMICOLON = "SYN_001";
    constexpr const char* SYN_EXPECTED_EXPRESSION = "SYN_002";
    constexpr const char* SYN_EXPECTED_TOKEN = "SYN_003";
    constexpr const char* SYN_UNEXPECTED_TOKEN = "SYN_004";
    constexpr const char* SYN_UNTERMINATED_STRING = "SYN_005";
    constexpr const char* SYN_INVALID_IDENTIFIER = "SYN_006";
    constexpr const char* SYN_KEYWORD_NOT_IMPLEMENTED = "SYN_007";
    
    // Runtime errors (RUN_001 - RUN_099)
    constexpr const char* RUN_DIVISION_BY_ZERO = "RUN_001";
    constexpr const char* RUN_UNHANDLED_EXCEPTION = "RUN_002";
    constexpr const char* RUN_STACK_OVERFLOW = "RUN_003";
    constexpr const char* RUN_OUT_OF_MEMORY = "RUN_004";
    
    // Type errors (TYP_001 - TYP_099)
    constexpr const char* TYP_INVALID_OPERATION = "TYP_001";
    constexpr const char* TYP_TYPE_MISMATCH = "TYP_002";
    constexpr const char* TYP_CANNOT_CONVERT = "TYP_003";
    
    // Reference errors (REF_001 - REF_099)
    constexpr const char* REF_UNDEFINED_VARIABLE = "REF_001";
    constexpr const char* REF_UNDEFINED_FUNCTION = "REF_002";
    constexpr const char* REF_UNDEFINED_PROPERTY = "REF_003";
    
    // Range errors (RNG_001 - RNG_099)
    constexpr const char* RNG_INDEX_OUT_OF_BOUNDS = "RNG_001";
    constexpr const char* RNG_INVALID_RANGE = "RNG_002";
}

#endif // SAPPHIRE_ERROR_H
