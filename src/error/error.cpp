#include "error.h"
#include "termcolor.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

std::string SapphireError::format() const {
    std::ostringstream oss;
    
    // Color code based on severity
    std::string severity_str;
    std::string color;
    
    switch (severity) {
        case ErrorSeverity::WARNING:
            severity_str = "Warning";
            color = tc_yellow();
            break;
        case ErrorSeverity::ERR:
            severity_str = "Error";
            color = tc_red();
            break;
        case ErrorSeverity::FATAL:
            severity_str = "Fatal Error";
            color = tc_red() + tc_bold();
            break;
    }
    
    // Type string
    std::string type_str;
    switch (type) {
        case ErrorType::SYNTAX_ERROR: type_str = "SyntaxError"; break;
        case ErrorType::RUNTIME_ERROR: type_str = "RuntimeError"; break;
        case ErrorType::TYPE_ERROR: type_str = "TypeError"; break;
        case ErrorType::REFERENCE_ERROR: type_str = "ReferenceError"; break;
        case ErrorType::RANGE_ERROR: type_str = "RangeError"; break;
        case ErrorType::NETWORK_ERROR: type_str = "NetworkError"; break;
        case ErrorType::DATABASE_ERROR: type_str = "DatabaseError"; break;
        case ErrorType::UI_ERROR: type_str = "UIError"; break;
        case ErrorType::INTERNAL_ERROR: type_str = "InternalError"; break;
    }
    
    oss << "\n" << color << "[" << type_str << " " << code << "] " << severity_str << tc_reset() << "\n";
    oss << "  " << tc_cyan() << message << tc_reset() << "\n";
    
    if (!location.file.empty()) {
        oss << "  at " << location.file << ":" << location.line << ":" << location.column << "\n";
    } else {
        oss << "  at line " << location.line << ":" << location.column << "\n";
    }
    
    // Show source line if available
    if (!location.source_line.empty()) {
        oss << "\n";
        oss << "  " << std::setw(4) << location.line << " | " << location.source_line << "\n";
        oss << "     | ";
        for (int i = 0; i < location.column - 1; i++) oss << " ";
        oss << color << "^";
        for (int i = 1; i < std::max(1, location.length); i++) oss << "~";
        oss << tc_reset() << "\n";
    }
    
    // Show technical message in verbose mode
    if (!technical_message.empty() && technical_message != message) {
        oss << "\n  " << tc_blue() << "Technical: " << technical_message << tc_reset() << "\n";
    }
    
    return oss.str();
}

std::string SapphireError::format_with_context() const {
    std::string result = format();
    
    // Show context
    if (!context.empty()) {
        result += "\n" + tc_yellow() + "Context:" + tc_reset() + "\n";
        for (const auto& ctx : context) {
            // Indent based on depth for nested context
            std::string indent = "";
            for (int i = 0; i < ctx.depth; i++) indent += "  ";
            
            result += indent + "• " + ctx.description;
            if (!ctx.value.empty()) {
                result += " = " + tc_cyan() + ctx.value + tc_reset();
            }
            if (!ctx.type.empty()) {
                result += " (" + tc_blue() + ctx.type + tc_reset() + ")";
            }
            result += "\n";
        }
    }
    
    // Show suggestions
    if (!suggestions.empty()) {
        // Sort by priority
        auto sorted_suggestions = suggestions;
        std::sort(sorted_suggestions.begin(), sorted_suggestions.end(),
                  [](const FixSuggestion& a, const FixSuggestion& b) {
                      return a.priority > b.priority;
                  });
        
        result += "\n" + tc_green() + "Suggestions:" + tc_reset() + "\n";
        for (const auto& sug : sorted_suggestions) {
            result += "  " + std::to_string(sug.priority) + ". " + sug.description + "\n";
            if (!sug.code_snippet.empty()) {
                result += "     " + tc_cyan() + sug.code_snippet + tc_reset() + "\n";
            }
        }
    }
    
    // Show stack trace if available
    if (!stack_trace.empty()) {
        result += "\n" + tc_blue() + "Stack trace:" + tc_reset() + "\n";
        result += stack_trace;
    }
    
    return result;
}

void ErrorHandler::report_error(std::shared_ptr<SapphireError> error) {
    errors.push_back(error);
    
    if (verbose) {
        if (show_suggestions) {
            std::cerr << error->format_with_context() << "\n";
        } else {
            std::cerr << error->format() << "\n";
        }
    }
}

void ErrorHandler::report_warning(std::shared_ptr<SapphireError> error) {
    errors.push_back(error);
    
    if (verbose) {
        if (show_suggestions) {
            std::cout << error->format_with_context() << "\n";
        } else {
            std::cout << error->format() << "\n";
        }
    }
}

void ErrorHandler::clear() {
    errors.clear();
}

bool ErrorHandler::has_errors() const {
    for (const auto& err : errors) {
        if (err->severity == ErrorSeverity::ERR || err->severity == ErrorSeverity::FATAL) {
            return true;
        }
    }
    return false;
}

bool ErrorHandler::has_warnings() const {
    for (const auto& err : errors) {
        if (err->severity == ErrorSeverity::WARNING) {
            return true;
        }
    }
    return false;
}

std::vector<std::shared_ptr<SapphireError>> ErrorHandler::get_errors() const {
    std::vector<std::shared_ptr<SapphireError>> result;
    for (const auto& err : errors) {
        if (err->severity == ErrorSeverity::ERR || err->severity == ErrorSeverity::FATAL) {
            result.push_back(err);
        }
    }
    return result;
}

std::vector<std::shared_ptr<SapphireError>> ErrorHandler::get_warnings() const {
    std::vector<std::shared_ptr<SapphireError>> result;
    for (const auto& err : errors) {
        if (err->severity == ErrorSeverity::WARNING) {
            result.push_back(err);
        }
    }
    return result;
}
