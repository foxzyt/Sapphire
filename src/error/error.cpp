#include "error.h"
#include <iostream>
#include <sstream>
#include <iomanip>

std::string SapphireError::format() const {
    std::ostringstream oss;
    
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
    
    oss << "[Line " << location.line << ":" << location.column << "] " << type_str << "\n";
    oss << "  " << message << "\n";
    
    // Show source line if available
    if (!location.source_line.empty()) {
        oss << "\n";
        oss << "  " << std::setw(4) << location.line << " | " << location.source_line << "\n";
        oss << "        ";
        for (int i = 0; i < location.column - 1; i++) oss << " ";
        oss << "^";
        for (int i = 1; i < std::max(1, location.length); i++) oss << "~";
        oss << "\n";
    }
    
    return oss.str();
}

std::string SapphireError::format_with_context() const {
    return format();
}

void ErrorHandler::report_error(std::shared_ptr<SapphireError> error) {
    errors.push_back(error);
    
    if (verbose) {
        std::cerr << error->format() << "\n";
    }
}

void ErrorHandler::report_warning(std::shared_ptr<SapphireError> error) {
    errors.push_back(error);
    
    if (verbose) {
        std::cout << error->format() << "\n";
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
